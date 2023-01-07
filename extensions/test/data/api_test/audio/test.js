// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Asserts that device property values match properties in |expectedProperties|.
 * The method will *not* assert that the device contains *only* properties
 * specified in expected properties.
 * @param {Object} expectedProperties Expected device properties.
 * @param {Object} device Device object to test.
 */
function assertDeviceMatches(expectedProperties, device) {
  Object.keys(expectedProperties).forEach(function(key) {
    chrome.test.assertEq(expectedProperties[key], device[key],
        'Property ' + key + ' of device ' + device.id);
  });
}

/**
 * Verifies that list of devices contains all and only devices from set of
 * expected devices. If will fail the test if an unexpected device is found.
 *
 * @param {Object.<string, Object>} expectedDevices Expected set of test
 *     devices. Maps device ID to device properties.
 * @param {Array.<Object>} devices List of input devices.
 */
function assertDevicesMatch(expectedDevices, devices) {
  var deviceIds = {};
  devices.forEach(function(device) {
    chrome.test.assertFalse(!!deviceIds[device.id],
                            'Duplicated device id: \'' + device.id + '\'.');
    deviceIds[device.id] = true;
  });

  function sortedKeys(obj) {
    return Object.keys(obj).sort();
  }
  chrome.test.assertEq(sortedKeys(expectedDevices), sortedKeys(deviceIds));

  devices.forEach(function(device) {
    assertDeviceMatches(expectedDevices[device.id], device);
  });
}

/**
 *
 * @param {Array.<Object>} devices List of devices returned by
 *    chrome.audio.getInfo or chrome.audio.getDevices.
 * @return {Object.<string, Object>} List of devices formatted as map of
 *      expected devices used to assert devices match expectation.
 */
function deviceListToExpectedDevicesMap(devices) {
  var expectedDevicesMap = {};
  devices.forEach(function(device) {
    expectedDevicesMap[device.id] = device;
  });
  return expectedDevicesMap;
}

/**
 * @param {Array.<Object>} devices List of devices returned by
 *    chrome.audio.getInfo or chrome.audio.getDevices.
 * @return {Array.<string>} Sorted list devices IDs for devices in |devices|.
 */
function getDeviceIds(devices) {
  return devices.map(function(device) {return device.id;}).sort();
}

var deviceChangedListener = null;

chrome.test.runTests([
  function getDevicesTest() {
    // Test output devices. Maps device ID -> tested device properties.
    var kTestDevices = {
      '30001': {
        id: '30001',
        stableDeviceId: '0',
        displayName: 'Jabra Speaker 1',
        deviceName: 'Jabra Speaker',
        streamType: 'OUTPUT'
      },
      '30002': {
        id: '30002',
        stableDeviceId: '1',
        displayName: 'Jabra Speaker 2',
        deviceName: 'Jabra Speaker',
        streamType: 'OUTPUT'
      },
      '30003': {
        id: '30003',
        stableDeviceId: '2',
        displayName: 'HDA Intel MID',
        deviceName: 'HDMI output',
        streamType: 'OUTPUT'
      },
      '40001': {
        id: '40001',
        stableDeviceId: '3',
        displayName: 'Jabra Mic 1',
        deviceName: 'Jabra Mic',
        streamType: 'INPUT'
      },
      '40002': {
        id: '40002',
        stableDeviceId: '4',
        displayName: 'Jabra Mic 2',
        deviceName: 'Jabra Mic',
        streamType: 'INPUT'
      },
      '40003': {
        id: '40003',
        stableDeviceId: '5',
        displayName: 'Logitech Webcam',
        deviceName: 'Webcam Mic',
        streamType: 'INPUT'
      }
    };

    chrome.audio.getDevices(chrome.test.callbackPass(function(devices) {
      assertDevicesMatch(kTestDevices, devices);
    }));
  },

  function getDevicesWithEmptyFilterTest() {
    // Test output devices. Maps device ID -> tested device properties.
    var kTestDevices = {
      '30001': {
        id: '30001',
        stableDeviceId: '0',
        displayName: 'Jabra Speaker 1',
        deviceName: 'Jabra Speaker',
        streamType: 'OUTPUT'
      },
      '30002': {
        id: '30002',
        stableDeviceId: '1',
        displayName: 'Jabra Speaker 2',
        deviceName: 'Jabra Speaker',
        streamType: 'OUTPUT'
      },
      '30003': {
        id: '30003',
        stableDeviceId: '2',
        displayName: 'HDA Intel MID',
        deviceName: 'HDMI output',
        streamType: 'OUTPUT'
      },
      '40001': {
        id: '40001',
        stableDeviceId: '3',
        displayName: 'Jabra Mic 1',
        deviceName: 'Jabra Mic',
        streamType: 'INPUT'
      },
      '40002': {
        id: '40002',
        stableDeviceId: '4',
        displayName: 'Jabra Mic 2',
        deviceName: 'Jabra Mic',
        streamType: 'INPUT'
      },
      '40003': {
        id: '40003',
        stableDeviceId: '5',
        displayName: 'Logitech Webcam',
        deviceName: 'Webcam Mic',
        streamType: 'INPUT'
      }
    };

    chrome.audio.getDevices({}, chrome.test.callbackPass(function(devices) {
      assertDevicesMatch(kTestDevices, devices);
    }));
  },

  function getInputDevicesTest() {
    var kTestDevices = {
      '40001': {
        id: '40001',
        streamType: 'INPUT'
      },
      '40002': {
        id: '40002',
        streamType: 'INPUT'
      },
      '40003': {
        id: '40003',
        streamType: 'INPUT'
      }
    };

    chrome.audio.getDevices({
      streamTypes: ['INPUT']
    }, chrome.test.callbackPass(function(devices) {
      assertDevicesMatch(kTestDevices, devices);
    }));
  },

  function getOutputDevicesTest() {
    var kTestDevices = {
      '30001': {
        id: '30001',
        streamType: 'OUTPUT'
      },
      '30002': {
        id: '30002',
        streamType: 'OUTPUT'
      },
      '30003': {
        id: '30003',
        streamType: 'OUTPUT'
      },
    };

    chrome.audio.getDevices({
      streamTypes: ['OUTPUT']
    }, chrome.test.callbackPass(function(devices) {
      assertDevicesMatch(kTestDevices, devices);
    }));
  },

  function getActiveDevicesTest() {
    chrome.audio.getDevices(chrome.test.callbackPass(function(initial) {
      var initialActiveDevices = initial.filter(function(device) {
        return device.isActive;
      });
      chrome.test.assertTrue(initialActiveDevices.length > 0);

      chrome.audio.getDevices({
        isActive: true
      }, chrome.test.callbackPass(function(devices) {
        assertDevicesMatch(
            deviceListToExpectedDevicesMap(initialActiveDevices),
            devices);
     }));

      var initialActiveInputs = initialActiveDevices.filter(function(device) {
        return device.streamType === 'INPUT';
      });
      chrome.test.assertTrue(initialActiveInputs.length > 0);

      chrome.audio.getDevices({
        isActive: true,
        streamTypes: ['INPUT']
      }, chrome.test.callbackPass(function(devices) {
        assertDevicesMatch(
            deviceListToExpectedDevicesMap(initialActiveInputs),
            devices);
      }));

      var initialActiveOutputs = initialActiveDevices.filter(function(device) {
        return device.streamType === 'OUTPUT';
      });
      chrome.test.assertTrue(initialActiveOutputs.length > 0);

      chrome.audio.getDevices({
        isActive: true,
        streamTypes: ['OUTPUT']
      }, chrome.test.callbackPass(function(devices) {
        assertDevicesMatch(
            deviceListToExpectedDevicesMap(initialActiveOutputs),
            devices);
      }));
    }));
  },

  function getInactiveDevicesTest() {
    chrome.audio.getDevices(chrome.test.callbackPass(function(initial) {
      var initialInactiveDevices = initial.filter(function(device) {
        return !device.isActive;
      });
      chrome.test.assertTrue(initialInactiveDevices.length > 0);

      chrome.audio.getDevices({
        isActive: false
      }, chrome.test.callbackPass(function(devices) {
        assertDevicesMatch(
            deviceListToExpectedDevicesMap(initialInactiveDevices),
            devices);
      }));
    }));
  },

  function setPropertiesTest() {
    chrome.audio.getDevices(chrome.test.callbackPass(function(initial) {
      var expectedDevices = deviceListToExpectedDevicesMap(initial);

      // Update expected input devices with values that should be changed in
      // test.
      var updatedInput = expectedDevices['40002'];
      chrome.test.assertFalse(updatedInput.gain === 65);
      updatedInput.level = 65;

      // Update expected output devices with values that should be changed in
      // test.
      var updatedOutput = expectedDevices['30001'];
      chrome.test.assertFalse(updatedOutput.volume === 45);
      updatedOutput.level = 45;

      chrome.audio.setProperties('30001', {
        level: 45
      }, chrome.test.callbackPass(function() {
        chrome.audio.setProperties('40002', {
          level: 65
        }, chrome.test.callbackPass(function() {
          chrome.audio.getDevices(chrome.test.callbackPass(function(devices) {
            assertDevicesMatch(expectedDevices, devices);
          }));
        }));
      }));
    }));
  },


  function inputMuteTest() {
    var getMute = function(callback) {
      chrome.audio.getMute('INPUT', chrome.test.callbackPass(callback));
    };
    getMute(function(originalValue) {
      chrome.audio.setMute(
          'INPUT', !originalValue, chrome.test.callbackPass(function() {
            getMute(function(value) {
              chrome.test.assertEq(!originalValue, value);
            });
          }));
    });
  },

  function outputMuteTest() {
    var getMute = function(callback) {
      chrome.audio.getMute('OUTPUT', chrome.test.callbackPass(callback));
    };
    getMute(function(originalValue) {
      chrome.audio.setMute(
          'OUTPUT', !originalValue, chrome.test.callbackPass(function() {
            getMute(function(value) {
              chrome.test.assertEq(!originalValue, value);
            });
          }));
    });
  },

  function setActiveDevicesTest() {
    chrome.audio.setActiveDevices({
      input: ['40002', '40003'],
      output: ['30001']
    }, chrome.test.callbackPass(function() {
      chrome.audio.getDevices({
        isActive: true
      }, chrome.test.callbackPass(function(activeDevices) {
        chrome.test.assertEq(['30001', '40002', '40003'],
                             getDeviceIds(activeDevices));
      }));
    }));
  },

  function setActiveDevicesOutputOnlyTest() {
    chrome.audio.getDevices({
      streamTypes: ['INPUT'],
      isActive: true
    }, chrome.test.callbackPass(function(initial) {
      var initialActiveInputs = getDeviceIds(initial);
      chrome.test.assertTrue(initialActiveInputs.length > 0);

      chrome.audio.setActiveDevices({
        output: ['30003']
      }, chrome.test.callbackPass(function() {
        chrome.audio.getDevices({
            isActive: true
        }, chrome.test.callbackPass(function(devices) {
          var expected = ['30003'].concat(initialActiveInputs).sort();
          chrome.test.assertEq(expected, getDeviceIds(devices));
        }));
      }));
    }));
  },

  function setActiveDevicesFailInputTest() {
    chrome.audio.getDevices({
      isActive: true
    }, chrome.test.callbackPass(function(initial) {
      var initialActiveIds = getDeviceIds(initial);
      chrome.test.assertTrue(initialActiveIds.length > 0);

      chrome.audio.setActiveDevices({
        input: ['0000000'],  /* does not exist */
        output: []
      }, chrome.test.callbackFail('Failed to set active devices.', function() {
        chrome.audio.getDevices({
          isActive: true
        }, chrome.test.callbackPass(function(devices) {
          chrome.test.assertEq(initialActiveIds, getDeviceIds(devices));
        }));
      }));
    }));
  },

  function setActiveDevicesFailOutputTest() {
    chrome.audio.getDevices({
      isActive: true
    }, chrome.test.callbackPass(function(initial) {
      var initialActiveIds = getDeviceIds(initial);
      chrome.test.assertTrue(initialActiveIds.length > 0);

      chrome.audio.setActiveDevices({
        input: [],
        output: ['40001'] /* id is input node ID */
      }, chrome.test.callbackFail('Failed to set active devices.', function() {
        chrome.audio.getDevices({
          isActive: true
        }, chrome.test.callbackPass(function(devices) {
          chrome.test.assertEq(initialActiveIds, getDeviceIds(devices));
        }));
      }));
    }));
  },

  function clearActiveDevicesTest() {
    chrome.audio.getDevices({
      isActive: true
    }, chrome.test.callbackPass(function(initial) {
      chrome.test.assertTrue(getDeviceIds(initial).length > 0);

      chrome.audio.setActiveDevices({
        input: [],
        output: []
      }, chrome.test.callbackPass(function() {
        chrome.audio.getDevices({
          isActive: true
        }, chrome.test.callbackPass(function(devices) {
          chrome.test.assertEq([], devices);
        }));
      }));
    }));
  },
]);
