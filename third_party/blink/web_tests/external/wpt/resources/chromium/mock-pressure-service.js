import {PressureState} from '/gen/services/device/public/mojom/pressure_update.mojom.m.js'
import {PressureService, PressureServiceReceiver, PressureStatus} from '/gen/third_party/blink/public/mojom/compute_pressure/pressure_service.mojom.m.js'

class MockPressureService {
  constructor() {
    this.receiver_ = new PressureServiceReceiver(this);
    this.interceptor_ =
        new MojoInterfaceInterceptor(PressureService.$interfaceName);
    this.interceptor_.oninterfacerequest = e => {
      this.receiver_.$.bindHandle(e.handle);
    };
    this.reset();
    this.mojomStateType_ = new Map([
      ['nominal', PressureState.kNominal], ['fair', PressureState.kFair],
      ['serious', PressureState.kSerious], ['critical', PressureState.kCritical]
    ]);
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
    this.pressureUpdate_ = null;
    this.pressureStatus_ = PressureStatus.kOk;
  }

  async bindObserver(observer) {
    if (this.observer_ !== null)
      throw new Error('BindObserver() has already been called');

    this.observer_ = observer;

    return {status: this.pressureStatus_};
  }

  sendUpdate() {
    if (this.pressureUpdate_ === null || this.observer_ === null)
      return;
    this.observer_.onUpdate(this.pressureUpdate_);
  }

  setPressureUpdate(state) {
    if (!this.mojomStateType_.has(state))
      throw new Error(`PressureState '${state}' is invalid`);

    this.pressureUpdate_ = {
      state: this.mojomStateType_.get(state),
      timestamp: window.performance.timeOrigin
    };
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
