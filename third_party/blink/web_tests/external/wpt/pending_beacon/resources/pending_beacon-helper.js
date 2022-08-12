'use strict';

function parallelPromiseTest(func, description) {
  async_test((t) => {
    Promise.resolve(func(t)).then(() => t.done()).catch(t.step_func((e) => {
      throw e;
    }));
  }, description);
}

/** @enum {string} */
const BeaconDataType = {
  String: 'String',
  ArrayBuffer: 'ArrayBuffer',
  FormData: 'FormData',
  URLSearchParams: 'URLSearchParams',
};

// Creates beacon data of the given `dataType` from `data`.
// @param {string} data - A string representation of the beacon data. Note that
//     it cannot contain UTF-16 surrogates for all `BeaconDataType` except BLOB.
// @param {BeaconDataType} dataType - must be one of `BeaconDataType`.
function makeBeaconData(data, dataType) {
  switch (dataType) {
    case BeaconDataType.String:
      return data;
    case BeaconDataType.ArrayBuffer:
      return new TextEncoder().encode(data).buffer;
    case BeaconDataType.FormData:
      const formData = new FormData();
      formData.append('payload', data);
      return formData;
    case BeaconDataType.URLSearchParams:
      return new URLSearchParams(data);
    default:
      throw Error(`Unsupported beacon dataType: ${dataType}`);
  }
}

// Create a string of `end`-`begin` characters, with characters starting from
// UTF-16 code unit `begin` to `end`-1.
function generateSequentialData(begin, end, skip) {
  const codeUnits = Array(end - begin).fill().map((el, i) => i + begin);
  if (skip) {
    return String.fromCharCode(
        ...codeUnits.filter(c => !skip.includes(String.fromCharCode(c))));
  }
  return String.fromCharCode(...codeUnits);
}

function generateSetBeaconCountURL(uuid) {
  return `/pending_beacon/resources/set_beacon_count.py?uuid=${uuid}`;
}

async function poll(f, expected) {
  const interval = 400;  // milliseconds.
  while (true) {
    const result = await f();
    if (result === expected) {
      return result;
    }
    await new Promise(resolve => setTimeout(resolve, interval));
  }
}

async function expectBeaconCount(uuid, expected) {
  poll(async () => {
    const res = await fetch(
        `/pending_beacon/resources/get_beacon_count.py?uuid=${uuid}`,
        {cache: 'no-store'});
    return await res.json();
  }, expected);
}

async function expectBeaconData(uuid, expected, options) {
  poll(async () => {
    const res = await fetch(
        `/pending_beacon/resources/get_beacon_count.py?uuid=${uuid}`,
        {cache: 'no-store'});
    let data = await res.text();
    if (options && options.percentDecoded) {
      // application/x-www-form-urlencoded serializer encodes space as '+'
      // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
      data = data.replace(/\+/g, '%20');
      return decodeURIComponent(data);
    }
    return data;
  }, expected);
}

function postBeaconSendDataTest(dataType, testData, expectEmpty, description) {
  parallelPromiseTest(async t => {
    const uuid = token();
    const url = `/pending_beacon/resources/set_beacon_data.py?uuid=${uuid}`;
    let beacon = new PendingPostBeacon(url);
    assert_equals(beacon.method, 'POST');

    beacon.setData(makeBeaconData(testData, dataType));
    beacon.sendNow();

    const expected = expectEmpty ? '<NO-DATA>' : testData;
    const percentDecoded = dataType === BeaconDataType.URLSearchParams;
    await expectBeaconData(uuid, expected, {percentDecoded: percentDecoded});
  }, `Beacon data of type "${dataType}": ${description}`);
}
