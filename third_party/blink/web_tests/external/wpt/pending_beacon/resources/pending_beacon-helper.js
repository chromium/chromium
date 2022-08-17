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
  Blob: 'Blob',
  File: 'File',
};

/** @enum {string} */
const BeaconDataTypeToSkipCharset = {
  String: '',
  ArrayBuffer: '',
  FormData: '\n\r',  // CRLF characters will be normalized by FormData
  URLSearchParams: ';,/?:@&=+$',  // reserved URI characters
  Blob: '',
  File: '',
};

const BEACON_PAYLOAD_KEY = 'payload';

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
      if (data.length > 0) {
        formData.append(BEACON_PAYLOAD_KEY, data);
      }
      return formData;
    case BeaconDataType.URLSearchParams:
      if (data.length > 0) {
        return new URLSearchParams(`${BEACON_PAYLOAD_KEY}=${data}`);
      }
      return new URLSearchParams();
    case BeaconDataType.Blob:
      return new Blob([data]);
    case BeaconDataType.File:
      return new File([data], 'file.txt', {type: 'text/plain'});
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

function generatePayload(size) {
  let data = '';
  if (size > 0) {
    const prefix = String(size) + ':';
    data = prefix + Array(size - prefix.length).fill('*').join('');
  }
  return data;
}

function generateSetBeaconURL(uuid) {
  return `/pending_beacon/resources/set_beacon.py?uuid=${uuid}`;
}

async function poll(f, expected) {
  const interval = 400;  // milliseconds.
  while (true) {
    const result = await f();
    if (expected(result)) {
      return result;
    }
    await new Promise(resolve => setTimeout(resolve, interval));
  }
}

// Waits until the `options.count` number of beacon data available from the
// server. Defaults to 1.
// If `options.data` is set, it will be used to compare with the data from the
// response.
async function expectBeacon(uuid, options) {
  const expectedCount = options && options.count ? options.count : 1;

  const res = await poll(
      async () => {
        const res = await fetch(
            `/pending_beacon/resources/get_beacon.py?uuid=${uuid}`,
            {cache: 'no-store'});
        return await res.json();
      },
      (res) => {
        return res.data.length == expectedCount;
      });
  if (!options || !options.data) {
    return;
  }

  const decoder = options && options.percentDecoded ? (s) => {
    // application/x-www-form-urlencoded serializer encodes space as '+'
    // https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/encodeURIComponent
    s = s.replace(/\+/g, '%20');
    return decodeURIComponent(s);
  } : (s) => s;

  assert_equals(
      res.data.length, options.data.length,
      `The size of beacon data ${
          res.data.length} from server does not match expected value ${
          options.data.length}.`);
  for (let i = 0; i < options.data.length; i++) {
    assert_equals(
        decoder(res.data[i]), options.data[i],
        'The beacon data does not match expected value.');
  }
}

function postBeaconSendDataTest(dataType, testData, description, options) {
  parallelPromiseTest(async t => {
    const expectNoData = options && options.expectNoData;
    const uuid = token();
    const url = generateSetBeaconURL(uuid);
    const beacon = new PendingPostBeacon(url);
    assert_equals(beacon.method, 'POST', 'must be POST to call setData().');

    beacon.setData(makeBeaconData(testData, dataType));
    beacon.sendNow();

    const expectedData = expectNoData ? null : testData;
    const percentDecoded =
        !expectNoData && dataType === BeaconDataType.URLSearchParams;
    await expectBeacon(
        uuid, {count: 1, data: [expectedData], percentDecoded: percentDecoded});
  }, `PendingPostBeacon(${dataType}): ${description}`);
}
