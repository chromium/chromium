// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_fusion.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/observer_list.h"
#include "base/ranges/algorithm.h"
#include "base/time/time.h"
#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"
#include "services/device/generic_sensor/platform_sensor_util.h"

namespace device {

class PlatformSensorFusion::Factory : public base::RefCounted<Factory> {
 public:
  static void CreateSensorFusion(
      SensorReadingSharedBuffer* reading_buffer,
      std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm,
      PlatformSensorProviderBase::CreateSensorCallback callback,
      PlatformSensorProvider* provider) {
    scoped_refptr<Factory> factory(new Factory(reading_buffer,
                                               std::move(fusion_algorithm),
                                               std::move(callback), provider));
    factory->FetchSources();
  }

 private:
  friend class base::RefCounted<Factory>;

  Factory(SensorReadingSharedBuffer* reading_buffer,
          std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm,
          PlatformSensorProviderBase::CreateSensorCallback callback,
          PlatformSensorProvider* provider)
      : fusion_algorithm_(std::move(fusion_algorithm)),
        result_callback_(std::move(callback)),
        reading_buffer_(reading_buffer),
        provider_(provider) {
    DCHECK(!fusion_algorithm_->source_types().empty());
    DCHECK(result_callback_);
    DCHECK(reading_buffer_);
    DCHECK(provider_);
  }

  ~Factory() = default;

  void FetchSources() {
    for (mojom::SensorType type : fusion_algorithm_->source_types()) {
      scoped_refptr<PlatformSensor> sensor = provider_->GetSensor(type);
      if (sensor) {
        SensorCreated(std::move(sensor));
      } else {
        provider_->CreateSensor(type,
                                base::BindOnce(&Factory::SensorCreated, this));
      }
    }
  }

  void SensorCreated(scoped_refptr<PlatformSensor> sensor) {
    if (!result_callback_) {
      // It is possible, if this callback has been already called
      // with nullptr (i.e. failed to fetch some of the required
      // source sensors). See the condition below.
      return;
    }

    if (!sensor) {
      std::move(result_callback_).Run(nullptr);
      return;
    }
    mojom::SensorType type = sensor->GetType();
    sources_map_[type] = std::move(sensor);
    if (sources_map_.size() == fusion_algorithm_->source_types().size()) {
      scoped_refptr<PlatformSensor> fusion_sensor(new PlatformSensorFusion(
          reading_buffer_, provider_, std::move(fusion_algorithm_),
          std::move(sources_map_)));
      std::move(result_callback_).Run(fusion_sensor);
    }
  }

  std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm_;
  PlatformSensorProviderBase::CreateSensorCallback result_callback_;
  raw_ptr<SensorReadingSharedBuffer>
      reading_buffer_;  // NOTE: Owned by |provider_|.
  raw_ptr<PlatformSensorProvider> provider_;
  PlatformSensorFusion::SourcesMap sources_map_;
};

// static
void PlatformSensorFusion::Create(
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider,
    std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm,
    PlatformSensorProviderBase::CreateSensorCallback callback) {
  Factory::CreateSensorFusion(reading_buffer, std::move(fusion_algorithm),
                              std::move(callback), provider);
}

PlatformSensorFusion::PlatformSensorFusion(
    SensorReadingSharedBuffer* reading_buffer,
    PlatformSensorProvider* provider,
    std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm,
    PlatformSensorFusion::SourcesMap sources)
    : PlatformSensor(fusion_algorithm->fused_type(), reading_buffer, provider),
      fusion_algorithm_(std::move(fusion_algorithm)),
      source_sensors_(std::move(sources)),
      reporting_mode_(mojom::ReportingMode::CONTINUOUS) {
  for (const auto& pair : source_sensors_)
    pair.second->AddClient(this);

  fusion_algorithm_->set_fusion_sensor(this);

  if (base::ranges::any_of(source_sensors_, [](const auto& pair) {
        return pair.second->GetReportingMode() ==
               mojom::ReportingMode::ON_CHANGE;
      })) {
    reporting_mode_ = mojom::ReportingMode::ON_CHANGE;
  }
}

PlatformSensorFusion::~PlatformSensorFusion() {
  for (const auto& pair : source_sensors_)
    pair.second->RemoveClient(this);
}

mojom::ReportingMode PlatformSensorFusion::GetReportingMode() {
  return reporting_mode_;
}

PlatformSensorConfiguration PlatformSensorFusion::GetDefaultConfiguration() {
  PlatformSensorConfiguration default_configuration;
  for (const auto& pair : source_sensors_) {
    double frequency = pair.second->GetDefaultConfiguration().frequency();
    if (frequency > default_configuration.frequency())
      default_configuration.set_frequency(frequency);
  }
  return default_configuration;
}

bool PlatformSensorFusion::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  // Remove all the previously added source configs.
  StopSensor();
  for (const auto& pair : source_sensors_) {
    if (!pair.second->StartListening(
            this, PlatformSensorConfiguration(
                      std::min(configuration.frequency(),
                               pair.second->GetMaximumSupportedFrequency())))) {
      StopSensor();
      return false;
    }
  }

  fusion_algorithm_->SetFrequency(configuration.frequency());
  return true;
}

void PlatformSensorFusion::StopSensor() {
  for (const auto& pair : source_sensors_)
    pair.second->StopListening(this);

  fusion_algorithm_->Reset();
}

bool PlatformSensorFusion::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  for (const auto& pair : source_sensors_) {
    if (!pair.second->CheckSensorConfiguration(PlatformSensorConfiguration(
            std::min(configuration.frequency(),
                     pair.second->GetMaximumSupportedFrequency()))))
      return false;
  }
  return true;
}

double PlatformSensorFusion::GetMaximumSupportedFrequency() {
  double maximum_frequency = 0.0;
  for (const auto& pair : source_sensors_) {
    maximum_frequency = std::max(maximum_frequency,
                                 pair.second->GetMaximumSupportedFrequency());
  }
  return maximum_frequency;
}

void PlatformSensorFusion::OnSensorReadingChanged(mojom::SensorType type) {
  SensorReading reading;
  reading.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();

  if (!fusion_algorithm_->GetFusedData(type, &reading))
    return;

  UpdateSharedBufferAndNotifyClients(reading);
}

void PlatformSensorFusion::OnSensorError() {
  NotifySensorError();
}

bool PlatformSensorFusion::IsSuspended() {
  for (auto& client : clients_) {
    if (!client.IsSuspended())
      return false;
  }
  return true;
}

bool PlatformSensorFusion::GetSourceReading(mojom::SensorType type,
                                            SensorReading* result) {
  auto it = source_sensors_.find(type);
  if (it != source_sensors_.end())
    return it->second->GetLatestRawReading(result);
  NOTREACHED();
  return false;
}

bool PlatformSensorFusion::IsSignificantlyDifferent(
    const SensorReading& reading1,
    const SensorReading& reading2,
    mojom::SensorType) {
  for (size_t i = 0; i < SensorReadingRaw::kValuesCount; ++i) {
    if (std::fabs(reading1.raw.values[i] - reading2.raw.values[i]) >=
        fusion_algorithm_->threshold()) {
      return true;
    }
  }
  return false;
}

}  // namespace device
