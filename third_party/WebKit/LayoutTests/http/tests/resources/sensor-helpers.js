'use strict';

// Wraps callback and calls rejectFunc if callback throws an error.
class CallbackWrapper {
  constructor(callback, rejectFunc) {
    this.wrapperFunc_ = (args) => {
      try {
        callback(args);
      } catch(e) {
        rejectFunc(e);
      }
    }
  }

  get callback() {
    return this.wrapperFunc_;
  }
}

function sensorMocks() {
  // Helper function that returns resolved promise with result.
  function sensorResponse(success) {
    return Promise.resolve({success});
  }

  // Class that mocks Sensor interface defined in sensor.mojom
  class MockSensor {
    constructor(sensorRequest, handle, offset, size, reportingMode) {
      this.client_ = null;
      this.startShouldFail_ = false;
      this.notifyOnReadingChange_ = true;
      this.reportingMode_ = reportingMode;
      this.sensorReadingTimerId_ = null;
      this.updateReadingFunction_ = null;
      this.suspendCalled_ = null;
      this.resumeCalled_ = null;
      this.addConfigurationCalled_ = null;
      this.removeConfigurationCalled_ = null;
      this.requestedFrequencies_ = [];
      let rv = handle.mapBuffer(offset, size);
      assert_equals(rv.result, Mojo.RESULT_OK, "Failed to map shared buffer");
      this.bufferArray_ = rv.buffer;
      this.buffer_ = new Float64Array(this.bufferArray_);
      this.resetBuffer();
      this.binding_ = new mojo.Binding(device.mojom.Sensor, this,
                                       sensorRequest);
      this.binding_.setConnectionErrorHandler(() => {
        this.reset();
      });
    }

    // Returns default configuration.
    getDefaultConfiguration() {
      return Promise.resolve({frequency: 5});
    }

    // Adds configuration for the sensor and starts reporting fake data
    // through updateReadingFunction_ callback.
    addConfiguration(configuration) {
      assert_not_equals(configuration, null, "Invalid sensor configuration.");

      this.requestedFrequencies_.push(configuration.frequency);
      // Sort using descending order.
      this.requestedFrequencies_.sort(
          (first, second) => { return second - first });

      if (!this.startShouldFail_ )
        this.startReading();

      if (this.addConfigurationCalled_ != null)
        this.addConfigurationCalled_(this);

      return sensorResponse(!this.startShouldFail_);
    }

    // Removes sensor configuration from the list of active configurations and
    // stops notification about sensor reading changes if
    // requestedFrequencies_ is empty.
    removeConfiguration(configuration) {
      if (this.removeConfigurationCalled_ != null) {
        this.removeConfigurationCalled_(this);
      }

      let index = this.requestedFrequencies_.indexOf(configuration.frequency);
      if (index == -1)
        return;

      this.requestedFrequencies_.splice(index, 1);
      if (this.requestedFrequencies_.length === 0)
        this.stopReading();
    }

    // Suspends sensor.
    suspend() {
      this.stopReading();
      if (this.suspendCalled_ != null) {
        this.suspendCalled_(this);
      }
    }

    // Resumes sensor.
    resume() {
      assert_equals(this.sensorReadingTimerId_, null);
      this.startReading();
      if (this.resumeCalled_ != null) {
        this.resumeCalled_(this);
      }
    }

    // Mock functions

    // Resets mock Sensor state.
    reset() {
      this.stopReading();

      this.startShouldFail_ = false;
      this.notifyOnReadingChange_ = true;
      this.updateReadingFunction_ = null;
      this.requestedFrequencies_ = [];
      this.suspendCalled_ = null;
      this.resumeCalled_ = null;
      this.addConfigurationCalled_ = null;
      this.removeConfigurationCalled_ = null;
      this.resetBuffer();
      this.bufferArray_ = null;
      this.binding_.close();
    }

    // Zeroes shared buffer.
    resetBuffer() {
      for (let i = 0; i < this.buffer_.length; ++i) {
        this.buffer_[i] = 0;
      }
    }

    // Sets callback that is used to deliver sensor reading updates.
    setUpdateSensorReadingFunction(updateReadingFunction) {
      this.updateReadingFunction_ = updateReadingFunction;
      return Promise.resolve(this);
    }

    // Sets flag that forces sensor to fail when addConfiguration is invoked.
    setStartShouldFail(shouldFail) {
      this.startShouldFail_ = shouldFail;
    }

    // Configures whether to report a reading change when in ON_CHANGE
    // reporting mode.
    configureReadingChangeNotifications(notifyOnReadingChange) {
      this.notifyOnReadingChange_ = notifyOnReadingChange;
    }

    // Returns resolved promise if suspend() was called, rejected otherwise.
    suspendCalled() {
      return new Promise((resolve, reject) => {
        this.suspendCalled_ = resolve;
      });
    }

    // Returns resolved promise if resume() was called, rejected otherwise.
    resumeCalled() {
      return new Promise((resolve, reject) => {
        this.resumeCalled_ = resolve;
      });
    }

    // Resolves promise when addConfiguration() is called.
    addConfigurationCalled() {
      return new Promise((resolve, reject) => {
        this.addConfigurationCalled_ = resolve;
      });
    }

    // Resolves promise when removeConfiguration() is called.
    removeConfigurationCalled() {
      return new Promise((resolve, reject) => {
        this.removeConfigurationCalled_ = resolve;
      });
    }

    startReading() {
      if (this.updateReadingFunction_ != null) {
        this.stopReading();
        let maxFrequencyUsed = this.requestedFrequencies_[0];
        let timeout = (1 / maxFrequencyUsed) * 1000;
        this.sensorReadingTimerId_ = window.setInterval(() => {
          if (this.updateReadingFunction_) {
            this.updateReadingFunction_(this.buffer_);
            // For all tests sensor reading should have monotonically
            // increasing timestamp in seconds.
            this.buffer_[1] = window.performance.now() * 0.001;
          }
          if (this.reportingMode_ === device.mojom.ReportingMode.ON_CHANGE &&
              this.notifyOnReadingChange_) {
            this.client_.sensorReadingChanged();
          }
        }, timeout);
      }
    }

    stopReading() {
      if (this.sensorReadingTimerId_ != null) {
        window.clearInterval(this.sensorReadingTimerId_);
        this.sensorReadingTimerId_ = null;
      }
    }

    getSamplingFrequency() {
       assert_true(this.requestedFrequencies_.length > 0);
       return this.requestedFrequencies_[0];
    }

  }

  // Class that mocks SensorProvider interface defined in
  // sensor_provider.mojom
  class MockSensorProvider {
    constructor() {
      this.readingSizeInBytes_ =
          device.mojom.SensorInitParams.kReadBufferSizeForTests;
      this.sharedBufferSizeInBytes_ = this.readingSizeInBytes_ *
              (device.mojom.SensorType.MAX_VALUE + 1);
      let rv = Mojo.createSharedBuffer(this.sharedBufferSizeInBytes_);
      assert_equals(rv.result, Mojo.RESULT_OK, "Failed to create buffer");
      this.sharedBufferHandle_ = rv.handle;
      this.activeSensors_ = new Map();
      this.resolveFuncs_ = new Map();
      this.getSensorShouldFail_ = new Map();
      this.permissionsDenied_ = new Map();
      this.isContinuous_ = false;
      this.maxFrequency_ = 60;
      this.minFrequency_ = 1;
      this.binding_ = new mojo.Binding(device.mojom.SensorProvider, this);

      this.interceptor_ = new MojoInterfaceInterceptor(
          device.mojom.SensorProvider.name);
      this.interceptor_.oninterfacerequest = e => {
        this.bindToPipe(e.handle);
      };
      this.interceptor_.start();
    }

    // Returns initialized Sensor proxy to the client.
    async getSensor(type) {
      if (this.getSensorShouldFail_.get(type)) {
        return {result: device.mojom.SensorCreationResult.ERROR_NOT_AVAILABLE,
                initParams: null};
      }
      if (this.permissionsDenied_.get(type)) {
        return {result: device.mojom.SensorCreationResult.ERROR_NOT_ALLOWED,
                initParams: null};
      }

      let offset = type * this.readingSizeInBytes_;
      let reportingMode = device.mojom.ReportingMode.ON_CHANGE;
      if (this.isContinuous_) {
        reportingMode = device.mojom.ReportingMode.CONTINUOUS;
      }

      let sensorPtr = new device.mojom.SensorPtr();
      if (!this.activeSensors_.has(type)) {
        let mockSensor = new MockSensor(
            mojo.makeRequest(sensorPtr), this.sharedBufferHandle_, offset,
            this.readingSizeInBytes_, reportingMode);
        this.activeSensors_.set(type, mockSensor);
        this.activeSensors_.get(type).client_ = new device.mojom.SensorClientPtr();
      }

      let rv = this.sharedBufferHandle_.duplicateBufferHandle();

      assert_equals(rv.result, Mojo.RESULT_OK);

      let defaultConfig = {frequency: 5};
      // Consider sensor traits to meet assertions in C++ code (see
      // services/device/public/cpp/generic_sensor/sensor_traits.h)
      if (type == device.mojom.SensorType.AMBIENT_LIGHT ||
          type == device.mojom.SensorType.MAGNETOMETER) {
        if (this.maxFrequency_ > 10)
          this.maxFrequency_ = 10;
      }

      let initParams = new device.mojom.SensorInitParams({
        sensor: sensorPtr,
        clientRequest: mojo.makeRequest(this.activeSensors_.get(type).client_),
        memory: rv.handle,
        bufferOffset: offset,
        mode: reportingMode,
        defaultConfiguration: defaultConfig,
        minimumFrequency: this.minFrequency_,
        maximumFrequency: this.maxFrequency_
      });

      if (this.resolveFuncs_.has(type)) {
        for (let resolveFunc of this.resolveFuncs_.get(type)) {
          resolveFunc(this.activeSensors_.get(type));
        }
        this.resolveFuncs_.delete(type);
      }

      return {result: device.mojom.SensorCreationResult.SUCCESS,
              initParams: initParams};
    }

    // Binds object to mojo message pipe
    bindToPipe(pipe) {
      this.binding_.bind(pipe);
      this.binding_.setConnectionErrorHandler(() => {
        this.reset();
      });
    }

    // Mock functions

    // Resets state of mock SensorProvider between test runs.
    reset() {
      for (const sensor of this.activeSensors_.values()) {
        sensor.reset();
      }
      this.activeSensors_.clear();
      this.resolveFuncs_.clear();
      this.getSensorShouldFail_.clear();
      this.permissionsDenied_.clear();
      this.maxFrequency_ = 60;
      this.minFrequency_ = 1;
      this.isContinuous_ = false;
      this.binding_.close();
      this.interceptor_.stop();
    }

    // Sets flag that forces mock SensorProvider to fail when getSensor() is
    // invoked.
    setGetSensorShouldFail(type, shouldFail) {
      this.getSensorShouldFail_.set(type, shouldFail);
    }

    setPermissionsDenied(type, permissionsDenied) {
      this.permissionsDenied_.set(type, permissionsDenied);
    }

    // Returns mock sensor that was created in getSensor to the layout test.
    getCreatedSensor(type) {
      assert_equals(typeof type, "number", "A sensor type must be specified.");

      if (this.activeSensors_.has(type)) {
        return Promise.resolve(this.activeSensors_.get(type));
      }

      return new Promise((resolve, reject) => {
        if (!this.resolveFuncs_.has(type)) {
          this.resolveFuncs_.set(type, []);
        }
        this.resolveFuncs_.get(type).push(resolve);
      });
    }

    // Forces sensor to use |reportingMode| as an update mode.
    setContinuousReportingMode() {
      this.isContinuous_ = true;
    }

    // Sets the maximum frequency for a concrete sensor.
    setMaximumSupportedFrequency(frequency) {
      this.maxFrequency_ = frequency;
    }

    // Sets the minimum frequency for a concrete sensor.
    setMinimumSupportedFrequency(frequency) {
      this.minFrequency_ = frequency;
    }
  }

  return new MockSensorProvider();
}

function sensor_test(func, name, properties) {
  promise_test(async () => {
    let sensorProvider = sensorMocks();

    // Clean up and reset mock sensor stubs asynchronously, so that the blink
    // side closes its proxies and notifies JS sensor objects before new test is
    // started.
    try {
      await func(sensorProvider);
    } finally {
      sensorProvider.reset();
      await new Promise(resolve => { setTimeout(resolve, 0); });
    };
  }, name, properties);
}

async function setMockSensorDataForType(sensorProvider, sensorType, mockDataArray) {
  let createdSensor = await sensorProvider.getCreatedSensor(sensorType);
  return createdSensor.setUpdateSensorReadingFunction(buffer => {
    buffer.set(mockDataArray, 2);
  });
}

// Returns a promise that will be resolved when an event equal to the given
// event is fired.
function waitForEvent(expectedEvent, targetWindow = window) {
  let stringify = (thing, targetWindow) => {
    if (thing instanceof targetWindow.Object && thing.constructor !== targetWindow.Object) {
      let str = '{';
      for (let key of Object.keys(Object.getPrototypeOf(thing))) {
        str += JSON.stringify(key) + ': ' + stringify(thing[key], targetWindow) + ', ';
      }
      return str + '}';
    } else if (thing instanceof Number) {
      return thing.toFixed(6);
    }
    return JSON.stringify(thing);
  };

  return new Promise((resolve, reject) => {
    let events = [];
    let timeoutId = null;

    let expectedEventString = stringify(expectedEvent, window);
    function listener(event) {
      let eventString = stringify(event, targetWindow);
      if (eventString === expectedEventString) {
        targetWindow.clearTimeout(timeoutId);
        targetWindow.removeEventListener(expectedEvent.type, listener);
        resolve();
      } else {
        events.push(eventString);
      }
    }
    targetWindow.addEventListener(expectedEvent.type, listener);

    timeoutId = targetWindow.setTimeout(() => {
      targetWindow.removeEventListener(expectedEvent.type, listener);
      let errorMessage = 'Timeout waiting for expected event: ' + expectedEventString;
      if (events.length == 0) {
        errorMessage += ', no events were fired';
      } else {
        errorMessage += ', received events: '
        for (let event of events) {
          errorMessage += event + ', ';
        }
      }
      reject(errorMessage);
    }, 500);
  });
}

// TODO(Mikhail): Refactor further to remove code duplication
// in <concrete sensor>.html files.
function verify_sensor_reading(pattern, values, timestamp, is_null) {
  function round(val) {
    return Number.parseFloat(val).toPrecision(6);
  }

  if (is_null) {
    return (values === null || values.every(r => r === null)) &&
           timestamp === null;
  }
  return values.every((r, i) => round(r) === round(pattern[i])) &&
         timestamp !== null;
}

function verify_xyz_sensor_reading(pattern, {x, y, z, timestamp}, is_null) {
  return verify_sensor_reading(pattern, [x, y, z], timestamp, is_null);
}

function verify_quat_sensor_reading(pattern, {quaternion, timestamp}, is_null) {
  return verify_sensor_reading(pattern, quaternion, timestamp, is_null);
}
