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
    // Sets a timestamp by creating a DOMHighResTimeStamp from a given
    // platform timestamp. In this mock implementation we use a starting value
    // and an increment step value that resemble a platform timestamp
    // reasonably enough.
    this.timestamp_ = window.performance.timeOrigin;
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.stopPlatformCollector();
    this.receiver_.$.close();
    this.interceptor_.stop();

    // Wait for an event loop iteration to let any pending mojo commands in
    // the pressure service finish.
    return new Promise(resolve => setTimeout(resolve, 0));
  }

  reset() {
    this.observer_ = null;
    this.pressureUpdate_ = null;
    this.pressureServiceReadingTimerId_ = null;
    this.pressureStatus_ = PressureStatus.kOk;
    this.updatesDelivered_ = 0;
  }

  async bindObserver(observer) {
    if (this.observer_ !== null)
      throw new Error('BindObserver() has already been called');

    this.observer_ = observer;

    return {status: this.pressureStatus_};
  }

  startPlatformCollector(sampleRate) {
    if (sampleRate === 0)
      return;

    if (this.pressureServiceReadingTimerId_ != null)
      stopPlatformCollector();

    const timeout = (1 / sampleRate) * 1000;
    this.pressureServiceReadingTimerId_ = window.setInterval(() => {
      this.sendUpdate();
    }, timeout);
  }

  stopPlatformCollector() {
    if (this.pressureServiceReadingTimerId_ != null) {
      window.clearInterval(this.pressureServiceReadingTimerId_);
      this.pressureServiceReadingTimerId_ = null;
    }
    this.updatesDelivered_ = 0;
  }

  updatesDelivered() {
    return this.updatesDelivered_;
  }

  sendUpdate() {
    if (this.pressureUpdate_ === null || this.observer_ === null)
      return;
    this.pressureUpdate_.timestamp = this.timestamp_++;
    this.observer_.onUpdate(this.pressureUpdate_);
    this.updatesDelivered_++;
  }

  setPressureUpdate(state) {
    if (!this.mojomStateType_.has(state))
      throw new Error(`PressureState '${state}' is invalid`);

    this.pressureUpdate_ = {
      state: this.mojomStateType_.get(state),
      timestamp: window.performance.timeOrigin
    };
  }
}

export const mockPressureService = new MockPressureService();
