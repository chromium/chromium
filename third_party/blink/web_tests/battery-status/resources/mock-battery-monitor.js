"use strict";

class BatteryMonitorImpl {
  constructor(mock) {
    this.mock_ = mock;
  }

  queryNextStatus() {
    return this.mock_.queryNextStatus();
  }
}

class MockBatteryMonitor {
  constructor() {
    this.pendingRequests_ = [];
    this.status_ = null;
    this.bindingSet_ = new mojo.BindingSet(device.mojom.BatteryMonitor);

    this.interceptor_ = new MojoInterfaceInterceptor(
        device.mojom.BatteryMonitor.name);
    this.interceptor_.oninterfacerequest = e => this.bindRequest(e.handle);
    this.interceptor_.start();
  }

  bindRequest(handle) {
    let impl = new BatteryMonitorImpl(this);
    this.bindingSet_.addBinding(impl, handle);
  }

  queryNextStatus() {
    let result = new Promise(resolve => this.pendingRequests_.push(resolve));
    this.runCallbacks_();
    return result;
  }

  updateBatteryStatus(status) {
    this.status_ = status;
    this.runCallbacks_();
  }

  runCallbacks_() {
    if (!this.status_ || !this.pendingRequests_.length)
      return;

    let result = {status: this.status_};
    while (this.pendingRequests_.length) {
      this.pendingRequests_.pop()(result);
    }
    this.status_ = null;
  }
}

let mockBatteryMonitor = new MockBatteryMonitor();

let batteryInfo;
let lastSetMockBatteryInfo;

function setAndFireMockBatteryInfo(charging, chargingTime, dischargingTime,
                                   level) {
  let status = new device.mojom.BatteryStatus();
  status.charging = charging;
  status.chargingTime = chargingTime;
  status.dischargingTime = dischargingTime;
  status.level = level;

  lastSetMockBatteryInfo = status;
  mockBatteryMonitor.updateBatteryStatus(status);
}

// compare obtained battery values with the mock values
function testIfBatteryStatusIsUpToDate(batteryManager) {
  assert_not_equals(batteryManager, undefined);
  assert_not_equals(lastSetMockBatteryInfo, undefined);
  assert_equals(batteryManager.charging, lastSetMockBatteryInfo.charging);
  assert_equals(
      batteryManager.chargingTime, lastSetMockBatteryInfo.chargingTime);
  assert_equals(
      batteryManager.dischargingTime, lastSetMockBatteryInfo.dischargingTime);
  assert_equals(batteryManager.level, lastSetMockBatteryInfo.level);
}
