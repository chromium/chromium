// Compare two DataViews byte-by-byte.
function compareDataViews(actual, expected) {
  assert_true(actual instanceof DataView, 'actual is DataView');
  assert_true(expected instanceof DataView, 'expected is DataView');
  assert_equals(actual.byteLength, expected.byteLength, 'lengths equal');
  for (let i = 0; i < expected.byteLength; ++i) {
    assert_equals(actual.getUint8(i), expected.getUint8(i),
                  `Mismatch at byte ${i}.`);
  }
}

// Returns a Promise that resolves once |device| receives an input report.
function oninputreport(device) {
  assert_true(device instanceof HIDDevice)
  return new Promise(resolve => { device.oninputreport = resolve; });
}

// Fake implementation of device.mojom.HidConnection. HidConnection represents
// an open connection to a HID device and can be used to send and receive
// reports.
class FakeHidConnection {
  constructor(client) {
    this.client_ = client;
    this.expectedWrites_ = [];
    this.expectedGetFeatureReports_ = [];
    this.expectedSendFeatureReports_ = [];
  }

  bind(request) {
    assert_equals(this.binding, undefined);
    this.binding = new mojo.Binding(device.mojom.HidConnection, this, request);
    this.binding.setConnectionErrorHandler(() => { this.binding = undefined; });
  }

  // Simulate an input report sent from the device to the host. The connection
  // client's onInputReport method will be called with the provided |reportId|
  // and |buffer|.
  simulateInputReport(reportId, reportData) {
    if (this.client_) {
      this.client_.onInputReport(reportId, reportData);
    }
  }

  // Specify the result for an expected call to write. If |success| is true the
  // write will be successful, otherwise it will simulate a failure. The
  // parameters of the next write call must match |reportId| and |buffer|.
  queueExpectedWrite(success, reportId, reportData) {
    this.expectedWrites_.push({
      params: { reportId: reportId, data: reportData },
      result: { success: success },
    });
  }

  // Specify the result for an expected call to getFeatureReport. If |success|
  // is true the operation is successful, otherwise it will simulate a failure.
  // The parameter of the next getFeatureReport call must match |reportId|.
  queueExpectedGetFeatureReport(success, reportId, reportData) {
    this.expectedGetFeatureReports_.push({
      params: { reportId: reportId, },
      result: { success: success, buffer: reportData },
    });
  }

  // Specify the result for an expected call to sendFeatureReport. If |success|
  // is true the operation is successful, otherwise it will simulate a failure.
  // The parameters of the next sendFeatureReport call must match |reportId| and
  // |buffer|.
  queueExpectedSendFeatureReport(success, reportId, reportData) {
    this.expectedSendFeatureReports_.push({
      params: { reportId: reportId, data: reportData },
      result: { success: success },
    });
  }

  // Asserts that there are no more expected operations.
  assertExpectationsMet() {
    assert_equals(this.expectedWrites_.length, 0);
    assert_equals(this.expectedGetFeatureReports_.length, 0);
    assert_equals(this.expectedSendFeatureReports_.length, 0);
  }

  // Implementation of HidConnection::Write. Causes an assertion failure if
  // there are no expected write operations, or if the parameters do not match
  // the expected call.
  async write(reportId, buffer) {
    let expectedWrite = this.expectedWrites_.shift();
    assert_not_equals(expectedWrite, undefined);
    assert_equals(reportId, expectedWrite.params.reportId);
    let actual = new Uint8Array(buffer);
    compareDataViews(
        new DataView(actual.buffer, actual.byteOffset),
        new DataView(expectedWrite.params.data.buffer,
                     expectedWrite.params.data.byteOffset));
    return expectedWrite.result;
  }

  // Implementation of HidConnection::GetFeatureReport. Causes an assertion
  // failure if there are no expected write operations, or if the parameters do
  // not match the expected call.
  async getFeatureReport(reportId) {
    let expectedGetFeatureReport = this.expectedGetFeatureReports_.shift();
    assert_not_equals(expectedGetFeatureReport, undefined);
    assert_equals(reportId, expectedGetFeatureReport.params.reportId);
    return expectedGetFeatureReport.result;
  }

  // Implementation of HidConnection::SendFeatureReport. Causes an assertion
  // failure if there are no expected write operations, or if the parameters do
  // not match the expected call.
  async sendFeatureReport(reportId, buffer) {
    let expectedSendFeatureReport = this.expectedSendFeatureReports_.shift();
    assert_not_equals(expectedSendFeatureReport, undefined);
    assert_equals(reportId, expectedSendFeatureReport.params.reportId);
    let actual = new Uint8Array(buffer);
    compareDataViews(
        new DataView(actual.buffer, actual.byteOffset),
        new DataView(expectedSendFeatureReport.params.data.buffer,
                     expectedSendFeatureReport.params.data.byteOffset));
    return expectedSendFeatureReport.result;
  }
}


// A fake implementation of the HidService mojo interface. HidService manages
// HID device access for clients in the render process. Typically, when a client
// requests access to a HID device a chooser dialog is shown with a list of
// available HID devices. Selecting a device from the chooser also grants
// permission for the client to access that device.
//
// The fake implementation allows tests to simulate connected devices. It also
// skips the chooser dialog and instead allows tests to specify which device
// should be selected. All devices are treated as if the user had already
// granted permission.
class FakeHidService {
  constructor() {
    this.interceptor_ =
        new MojoInterfaceInterceptor(blink.mojom.HidService.name, "context", true);
    this.interceptor_.oninterfacerequest = e => this.bind(e.handle);
    this.bindingSet_ = new mojo.BindingSet(blink.mojom.HidService);
    this.nextGuidValue_ = 0;
    this.reset();
  }

  start() {
    this.interceptor_.start();
  }

  stop() {
    this.interceptor_.stop();
  }

  reset() {
    this.devices_ = new Map();
    this.fakeConnections_ = new Map();
    this.selectedDevice_ = null;
  }

  // Creates and returns a HidDeviceInfo with the specified device IDs.
  makeDevice(vendorId, productId) {
    let guidValue = ++this.nextGuidValue_;
    let info = new device.mojom.HidDeviceInfo();
    info.guid = guidValue.toString();
    info.vendorId = vendorId;
    info.productId = productId;
    info.productName = 'product name';
    info.serialNumber = '0';
    info.reportDescriptor = new Uint8Array();
    info.collections = [];
    info.deviceNode = 'device node';
    return info;
  }

  // Simulates a connected device. Returns the device GUID. A device connection
  // event is not generated.
  addDevice(deviceInfo) {
    this.devices_.set(deviceInfo.guid, deviceInfo);
    return deviceInfo.guid;
  }

  // Simulates disconnecting a connected device. A device disconnection event is
  // not generated.
  removeDevice(guid) {
    this.devices_.delete(guid);
  }

  // Sets the GUID of the device that will be returned as the selected item the
  // next time requestDevice is called. The device with this GUID must have been
  // previously added with addDevice.
  setSelectedDevice(guid) {
    this.selectedDevice_ = this.devices_.get(guid);
  }

  // Returns the fake HidConnection object for this device, if there is one. A
  // connection is created once the device is opened.
  getFakeConnection(guid) {
    return this.fakeConnections_.get(guid);
  }

  bind(handle) {
    this.bindingSet_.addBinding(this, handle);
  }

  // Returns an array of connected devices. Normally this would only include
  // devices that the client has already been granted permission to access, but
  // for the fake implementation all simulated devices are returned.
  async getDevices() {
    return { devices: Array.from(this.devices_.values()) };
  }

  // Simulates a device chooser prompt, returning |selectedDevice_| as the
  // simulated selection. |filters| is ignored.
  async requestDevice(filters) {
    return { device: this.selectedDevice_ };
  }

  // Returns a fake connection to the device with the specified GUID. If
  // |connectionClient| is not null, its onInputReport method will be called
  // when input reports are received.
  async connect(guid, connectionClient) {
    let fakeConnection = new FakeHidConnection(connectionClient);
    let connectionPtr = new device.mojom.HidConnectionPtr();
    fakeConnection.bind(mojo.makeRequest(connectionPtr));
    this.fakeConnections_.set(guid, fakeConnection);
    return { connection: connectionPtr };
  }
}

let fakeHidService = new FakeHidService();

function hid_test(func, name, properties) {
  promise_test(async (test) => {
    fakeHidService.start();
    try {
      await func(test, fakeHidService);
    } finally {
      fakeHidService.stop();
      fakeHidService.reset();
    }
  }, name, properties);
}

function trustedClick() {
  return new Promise(resolve => {
    let button = document.createElement('button');
    button.textContent = 'click to continue test';
    button.style.display = 'block';
    button.style.fontSize = '20px';
    button.style.padding = '10px';
    button.onclick = () => {
      document.body.removeChild(button);
      resolve();
    };
    document.body.appendChild(button);
    test_driver.click(button);
  });
}
