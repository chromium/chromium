// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_
#define SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_

#include <list>
#include <map>
#include <memory>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/single_thread_task_runner.h"
#include "mojo/public/cpp/system/buffer.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"
#include "services/device/public/mojom/sensor.mojom.h"

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

  virtual mojom::ReportingMode GetReportingMode() = 0;
  virtual PlatformSensorConfiguration GetDefaultConfiguration() = 0;
  virtual bool CheckSensorConfiguration(
      const PlatformSensorConfiguration& configuration) = 0;

  // Can be overriden to return the sensor maximum sampling frequency
  // value obtained from the platform if it is available. If platfrom
  // does not provide maximum sampling frequency this method must
  // return default frequency.
  // The default implementation returns default frequency.
  virtual double GetMaximumSupportedFrequency();

  // Can be overriden to return the sensor minimum sampling frequency.
  // The default implementation returns '1.0 / (60 * 60)', i.e. once per hour.
  virtual double GetMinimumSupportedFrequency();

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
  // Returns 'true' if the sensor is started; returns 'false' otherwise.
  bool IsActiveForTesting() const;
  using ConfigMap = std::map<Client*, std::list<PlatformSensorConfiguration>>;
  const ConfigMap& GetConfigMapForTesting() const;

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
  // Updates shared buffer with new sensor reading data and schedules
  // NotifySensorReadingChanged invocation on IPC thread.
  // Note: this method is thread-safe.
  void UpdateSharedBufferAndNotifyClients(const SensorReading& reading);

  // Updates shared buffer with provided SensorReading
  void UpdateSharedBuffer(const SensorReading& reading);

  void NotifySensorReadingChanged();
  void NotifySensorError();

  // Task runner that is used by mojo objects for the IPC.
  // If platfrom sensor events are processed on a different
  // thread, notifications are forwarded to |task_runner_|.
  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;
  base::ObserverList<Client, true>::Unchecked clients_;

 private:
  friend class base::RefCountedThreadSafe<PlatformSensor>;
  SensorReadingSharedBuffer* reading_buffer_;  // NOTE: Owned by |provider_|.
  mojom::SensorType type_;
  ConfigMap config_map_;
  PlatformSensorProvider* provider_;
  bool is_active_ = false;
  base::WeakPtrFactory<PlatformSensor> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(PlatformSensor);
};

}  // namespace device

#endif  // SERVICES_DEVICE_GENERIC_SENSOR_PLATFORM_SENSOR_H_
