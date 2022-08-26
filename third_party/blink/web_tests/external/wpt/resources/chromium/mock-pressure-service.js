import {PressureService, PressureServiceReceiver, PressureStatus, SetQuantizationStatus} from '/gen/third_party/blink/public/mojom/compute_pressure/pressure_service.mojom.m.js'

class MockPressureService {
  constructor() {
    this.receiver_ = new PressureServiceReceiver(this);
    this.interceptor_ =
        new MojoInterfaceInterceptor(PressureService.$interfaceName);
    this.interceptor_.oninterfacerequest = e => {
      this.receiver_.$.bindHandle(e.handle);
    };
    this.reset();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.receiver_.$.close();
    this.interceptor_.stop();

    // Wait for an event loop iteration to let any pending mojo commands in
    // the pressure service finish.
    return new Promise(resolve => setTimeout(resolve, 0));
  }

  reset() {
    this.observer_ = null;
    this.pressureState_ = null;
    this.quantization_ = null;
    this.pressureStatus_ = PressureStatus.kOk;
  }

  async bindObserver(observer) {
    if (this.observer_ !== null)
      throw new Error('BindObserver() has already been called');

    this.observer_ = observer;

    return {status: this.pressureStatus_};
  }

  isSameQuantization(quantization) {
    if (this.quantization_ === null)
      return false;

    if (quantization.cpuUtilizationThresholds.length !=
        this.quantization_.cpuUtilizationThresholds.length) {
      return false;
    }

    for (let i = 0; i < quantization.cpuUtilizationThresholds.length; i++) {
      if (quantization.cpuUtilizationThresholds[i] !=
          this.quantization_.cpuUtilizationThresholds[i]) {
        return false;
      }
    }

    return true;
  }

  async setQuantization(quantization) {
    if (this.isSameQuantization(quantization)) {
      return {status: SetQuantizationStatus.kUnchanged};
    } else {
      this.quantization_ = quantization;
      return {status: SetQuantizationStatus.kChanged};
    }
  }

  sendUpdate() {
    if (this.pressureState_ === null || this.observer_ === null)
      return;
    this.observer_.onUpdate(this.pressureState_);
  }

  setPressureState(value) {
    this.pressureState_ = value;
  }

  setExpectedFailure(expectedException) {
    assert_true(
        expectedException instanceof DOMException,
        'setExpectedFailure() expects a DOMException instance');
    if (expectedException.name === 'SecurityError') {
      this.pressureStatus_ = PressureStatus.kSecurityError;
    } else if (expectedException.name === 'NotSupportedError') {
      this.pressureStatus_ = PressureStatus.kNotSupported;
    } else {
      throw new TypeError(
          `Unexpected DOMException '${expectedException.name}'`);
    }
  }
}

export const mockPressureService = new MockPressureService();
