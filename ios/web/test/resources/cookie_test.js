// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview Add functionality used in tests.
 */

/**
 * Namespace for this file. It depends on |__gCrWeb| having already been
 * injected.
 */
__gCrWeb.cookieTest = {};

// Store namespace object in a global __gCrWeb object referenced by a
// string, so it does not get renamed by closure compiler during the
// minification.
__gCrWeb['cookieTest'] = __gCrWeb.cookieTest;

__gCrWeb.cookieTest.getCookies = function() {
  return document.cookie;
};

__gCrWeb.cookieTest.setCookie = function(newCookie) {
  document.cookie = newCookie;
};

// Returns the localStorage entry for key. If there is an DOMException,
// returns a dictionary { message: message }.
__gCrWeb.cookieTest.getLocalStorage = function(key) {
  try {
    if (localStorage.getItem(key) == null) {
      return 'a';
    }
    return localStorage.getItem(key);
  } catch (error) {
    if (error instanceof DOMException) {
      return {'message': error.message};
    }
    return null;
  }
};

__gCrWeb.cookieTest.setLocalStorage = function(key, value) {
  try {
    localStorage.setItem(key, value);
    return true;
  } catch (error) {
    if (error instanceof DOMException) {
      return {'message': error.message};
    }
    return {'message': error.message};
  }
};

// Returns the sessionStorage entry for key. If there is an DOMException,
// returns a dictionary { message: message }.
__gCrWeb.cookieTest.getSessionStorage = function(key) {
  try {
    return sessionStorage.getItem(key);
  } catch (error) {
    if (error instanceof DOMException) {
      return {'message': error.message};
    }
    return null;
  }
};

__gCrWeb.cookieTest.setSessionStorage = function(key, value) {
  try {
    sessionStorage.setItem(key, value);
    return true;
  } catch (error) {
    if (error instanceof DOMException) {
      return {'message': error.message};
    }
    return false;
  }
};

function onError(error) {
  if (error instanceof DOMException || error instanceof ReferenceError) {
    __gCrWeb.message.invokeOnHost({
      command: 'cookieTest.result',
      result: {message: error.message}
    });
    return;
  }
  __gCrWeb.message.invokeOnHost(
      {command: 'cookieTest.result', result: false});
}

async function asyncGetWrapper(key, getter) {
  try {
    const value = await getter(key);
    __gCrWeb.message.invokeOnHost(
        {command: 'cookieTest.result', result: value})
  } catch (error) {
    onError(error);
  }
}

async function asyncSetWrapper(key, value, setter) {
  try {
    await setter(key, value);
    __gCrWeb.message.invokeOnHost(
        {command: 'cookieTest.result', result: true})
  } catch (error) {
    onError(error);
  }
}

async function setCache(key, value) {
  const cache = await caches.open('cache');
  return cache.put(`/${key}`, new Response(value));
}

async function getCache(key) {
  const cache = await caches.open('cache');
  const result = await cache.match(new Request(`/${key}`));
  return result && result.text();
}

__gCrWeb.cookieTest.setCache = function(key, value) {
  asyncSetWrapper(key, value, setCache);
  return true;
};

__gCrWeb.cookieTest.getCache = function(key) {
  asyncGetWrapper(key, getCache);
  return 'This is an async function.';
};

function setIndexedDB(key, value) {
  return new Promise((resolve, reject) => {
    let open = indexedDB.open('db', 1);
    open.onupgradeneeded = () => {
      open.result.createObjectStore('store', {keyPath: 'id'});
    };
    open.onsuccess = () => {
      let db = open.result;
      var tx = db.transaction('store', 'readwrite');
      var store = tx.objectStore('store');
      store.put({id: key, value: value});
      tx.oncomplete = () => {
        db.close();
        resolve();
      };
    };
    open.onerror = reject;
  });
}

function getIndexedDB(key) {
  return new Promise((resolve, reject) => {
    let open = indexedDB.open('db');
    open.onsuccess = () => {
      let db = open.result;
      var hasStore = open.result.objectStoreNames.contains('store');
      if (!hasStore) {
        db.close();
        resolve();
        return;
      }
      var tx = db.transaction('store', 'readwrite');
      var store = tx.objectStore('store');

      var getResult = store.get(key);
      getResult.onsuccess = () =>
          resolve(getResult.result && getResult.result.value);

      tx.oncomplete = () => {
        db.close();
      };
    };
    open.onerror = reject;
  });
}

__gCrWeb.cookieTest.getIndexedDB = function(key) {
  asyncGetWrapper(key, getIndexedDB);
  return 'This is an async function.';
};

__gCrWeb.cookieTest.setIndexedDB = function(key, value) {
  asyncSetWrapper(key, value, setIndexedDB);
  return true;
};
