// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_

#include <list>
#include <map>
#include <memory>

#include "base/callback_forward.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/synchronization/lock.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace device {

class PlatformSensorProvider;
class PlatformSensorConfiguration;

// Base class for the sensors provided by the platform. Concrete instances of
// this class are created by platform specific PlatformSensorProvider.
class PlatformSensor : public base::RefCountedThreadSafe<PlatformSensor> {
 public:
  // The interface that must be implemented by PlatformSensor clients.
  class Client {
   public:
    virtual void OnSensorReadingChanged(mojom::SensorType type) = 0;
    virtual void OnSensorError() = 0;
    virtual bool IsSuspended() = 0;

   protected:
    virtual ~Client() {}
  };

  PlatformSensor(const PlatformSensor&) = delete;
  PlatformSensor& operator=(const PlatformSensor&) = delete;

  virtual mojom::ReportingMode GetReportingMode() = 0;
  virtual PlatformSensorConfiguration GetDefaultConfiguration() = 0;
  virtual bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) = 0;

  // Can be overridden to return the sensor maximum sampling frequency
  // value obtained from the platform if it is available. If platform
  // does not provide maximum sampling frequency this method must
  // return default frequency.
  // The default implementation returns default frequency.
  virtual double GetMaximumSupportedFrequency();

  // Can be overridden to return the sensor minimum sampling frequency.
  // The default implementation returns '1.0 / (60 * 60)', i.e. once per hour.
  virtual double GetMinimumSupportedFrequency();

  // Can be overridden to reset this sensor by the PlatformSensorProvider.
  virtual void SensorReplaced();

  // Checks if new value is significantly different than old value.
  // When the reading we get does not differ significantly from our current
  // value, we discard this reading and do not emit any events. This is a
  // privacy measure to avoid giving readings that are too specific.
  virtual bool IsSignificantlyDifferent(const SensorReading& lhs,
                                        const SensorReading& rhs,
                                        mojom::SensorType sensor_type);

  mojom::SensorType GetType() const;

  bool StartListening(Client* client,
                      const PlatformSensorConfiguration& config);
  bool StopListening(Client* client, const PlatformSensorConfiguration& config);
  // Stops all the configurations tied to the |client|, but the |client| still
  // gets notification.
  bool StopListening(Client* client);

  void UpdateSensor();

  void AddClient(Client*);
  void RemoveClient(Client*);

  bool GetLatestReading(SensorReading* result);
  // Return the last raw (i.e. unrounded) sensor reading.
  bool GetLatestRawReading(SensorReading* result) const;
  // Returns 'true' if the sensor is started; returns 'false' otherwise.
  bool IsActiveForTesting() const;
  using ConfigMap = std::map<Client*, std::list<PlatformSensorConfiguration>>;
  const ConfigMap& GetConfigMapForTesting() const;

  // Called by API users to post a task on |main_task_runner_| when run from a
  // different sequence.
  void PostTaskToMainSequence(const base::Location& location,
                              base::OnceClosure task);

 protected:
  virtual ~PlatformSensor();
  PlatformSensor(mojom::SensorType type,
                 SensorReadingSharedBuffer* reading_buffer,
                 PlatformSensorProvider* provider);

  using ReadingBuffer = SensorReadingSharedBuffer;

  virtual bool UpdateSensorInternal(const ConfigMap& configurations);
  virtual bool StartSensor(
      const PlatformSensorConfiguration& configuration) = 0;
  virtual void StopSensor() = 0;

  // Updates the shared buffer with new sensor reading data and posts a task to
  // invoke NotifySensorReadingChanged() on |main_task_runner_|.
  // Note: this method is thread-safe.
  void UpdateSharedBufferAndNotifyClients(const SensorReading& reading);

  void NotifySensorReadingChanged();
  void NotifySensorError();

  void ResetReadingBuffer();

  // Returns the task runner where this object has been created. Subclasses can
  // use it to post tasks to the right sequence when running on a different task
  // runner.
  scoped_refptr<base::SequencedTaskRunner> main_task_runner() const {
    return main_task_runner_;
  }

  base::ObserverList<Client, true>::Unchecked clients_;

 private:
  friend class base::RefCountedThreadSafe<PlatformSensor>;

  // Updates shared buffer with provided SensorReading. If
  // |do_significance_check| is true then |last_raw_reading_| and
  // |reading_buffer_| are only updated if |reading| is significantly different
  // from |last_raw_reading_|. Returns true if |reading_buffer_| has been
  // updated, and false otherwise.
  // Note: this method is thread-safe.
  bool UpdateSharedBuffer(const SensorReading& reading,
                          bool do_significance_check);

  scoped_refptr<base::SequencedTaskRunner> main_task_runner_;

  raw_ptr<SensorReadingSharedBuffer>
      reading_buffer_;  // NOTE: Owned by |provider_|.
  mojom::SensorType type_;
  ConfigMap config_map_;
  raw_ptr<PlatformSensorProvider> provider_;
  bool is_active_ = false;
  absl::optional<SensorReading> last_raw_reading_ GUARDED_BY(lock_);
  absl::optional<SensorReading> last_rounded_reading_ GUARDED_BY(lock_);
  // Protect last_raw_reading_ & last_rounded_reading_.
  mutable base::Lock lock_;
  base::WeakPtrFactory<PlatformSensor> weak_factory_{this};
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_
