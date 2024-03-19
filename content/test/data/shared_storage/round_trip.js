// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function getKeyValue(data) {
  let key = null;
  if (data.hasOwnProperty('key')) {
    key = data['key'];
  } else if (
      data.hasOwnProperty('keyCharCodeArray') &&
      Array.isArray(data['keyCharCodeArray'])) {
    key = String.fromCharCode.apply(this, data['keyCharCodeArray']);
  }
  if (!key) {
    console.log(
        '`data` does not have `key` or `keyCharCodeArray`,' +
        ' or `keyCharCodeArray` is not an Array.');
    return null;
  }

  let value = null;
  if (data.hasOwnProperty('value')) {
    value = data['value'];
  } else if (
      data.hasOwnProperty('valueCharCodeArray') &&
      Array.isArray(data['valueCharCodeArray'])) {
    value = String.fromCharCode.apply(this, data['valueCharCodeArray']);
  }
  if (!value) {
    console.log(
        '`data` does not have `value` or `valueCharCodeArray`,' +
        ' or `valueCharCodeArray` is not an Array.');
    return null;
  }
  return {key, value};
}

async function sharedStorageSet(data) {
  let keyValue = await getKeyValue(data);
  if (!keyValue) {
    return null;
  }
  let {key, value} = keyValue;
  await sharedStorage.set(key, value);
  return {key, value};
}

async function sharedStorageAppend(data) {
  let keyValue = await getKeyValue(data);
  if (!keyValue) {
    return null;
  }
  let {key, value} = keyValue;
  await sharedStorage.append(key, value);
  return {key, value};
}

class SetGetOperation {
  async run(data) {
    if (!data) {
      console.log('No `data`');
      return;
    }
    await sharedStorage.clear();

    let setResult = await sharedStorageSet(data);
    if (!setResult) {
      console.log('key or value missing');
      return;
    }
    let {key, value} = setResult;

    let retrieved = await sharedStorage.get(key);
    if (!retrieved) {
      console.log(`no value retrieved`);
      console.log(`original value was retrieved: false`);
      return;
    }
    const equalsSet = (retrieved === value);
    console.log(`original value was retrieved: ${equalsSet}`);
  }
}

class SetKeysOperation {
  async run(data) {
    if (!data) {
      console.log('No `data`');
      return;
    }
    await sharedStorage.clear();

    let setResult = await sharedStorageSet(data);
    if (!setResult) {
      console.log('key or value missing');
      return;
    }
    let {key, value} = setResult;

    let count = 0;
    for await (const retrievedKey of sharedStorage.keys()) {
      const equalsSet = (retrievedKey === key);
      console.log(`original key was retrieved: ${equalsSet}`);
      count++;
    }
    if (count !== 1) {
      console.log(`Error: expected 1 key, found ${count} keys.`);
    }
  }
}

class AppendDeleteOperation {
  async run(data) {
    if (!data) {
      console.log('No `data`');
      return;
    }
    await sharedStorage.clear();

    let appendResult = await sharedStorageAppend(data);
    if (!appendResult) {
      console.log('key or value missing');
      return;
    }
    let {key, value} = appendResult;

    let lengthBeforeDelete = await sharedStorage.length();
    if (lengthBeforeDelete !== 1) {
      console.log(`Error: expected 1 key, found ${lengthBeforeDelete} keys.`);
    }
    await sharedStorage.delete(key);
    let lengthAfterDelete = await sharedStorage.length();
    let deleteSuccess = (lengthBeforeDelete === lengthAfterDelete + 1);
    console.log(`delete success: ${deleteSuccess}`);
  }
}

class AppendEntriesOperation {
  async run(data) {
    if (!data) {
      console.log('No `data`');
      return;
    }
    await sharedStorage.clear();

    let appendResult = await sharedStorageAppend(data);
    if (!appendResult) {
      console.log('key or value missing');
      return;
    }
    let {key, value} = appendResult;

    let count = 0;
    for await (
        const [retrievedKey, retrievedValue] of sharedStorage.entries()) {
      const equalsSet = (retrievedKey === key && retrievedValue === value);
      console.log(`original entry was retrieved: ${equalsSet}`);
      count++;
    }
    if (count !== 1) {
      console.log(`Error: expected 1 entry, found ${count} entries.`);
    }
  }
}

register('set-get-operation', SetGetOperation);
register('set-keys-operation', SetKeysOperation);
register('append-delete-operation', AppendDeleteOperation);
register('append-entries-operation', AppendEntriesOperation);
