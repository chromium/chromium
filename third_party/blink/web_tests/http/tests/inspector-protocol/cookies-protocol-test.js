(async function(testRunner) {
  var {page, session, dp} = await testRunner.startBlank(
      `Tests that cookies are set, updated and removed.`);

  async function logCookies(success) {
    if (success !== undefined)
      testRunner.log('Success: ' + success);
    var data = (await dp.Network.getAllCookies()).result;
    testRunner.log('Num of cookies ' + data.cookies.length);
    data.cookies.sort((a, b) => a.name.localeCompare(b.name));
    for (var cookie of data.cookies) {
      var suffix = ''
      if (cookie.secure)
        suffix += `, secure`;
      if (cookie.httpOnly)
        suffix += `, httpOnly`;
      if (cookie.session)
        suffix += `, session`;
      if (cookie.sameSite)
        suffix += `, ${cookie.sameSite}`;
      if (cookie.expires !== -1)
        suffix += `, expires`;
      testRunner.log(`name: ${cookie.name}, value: ${cookie.value}, domain: ${cookie.domain}, path: ${cookie.path}${suffix}`);
    }
  }

  async function setCookie(cookie) {
    testRunner.log('Setting Cookie');
    var response = await dp.Network.setCookie(cookie);
    await logCookies(response.result.success);
  }

  async function deleteCookie(cookie) {
    testRunner.log('Deleting Cookie');
    await dp.Network.deleteCookies(cookie);
    await logCookies();
  }

  async function setCookies(cookies) {
    testRunner.log('Adding multiple cookies');
    var response = await dp.Network.setCookies({cookies});
    await logCookies();
  }

  async function deleteAllCookies() {
    var data = (await dp.Network.getAllCookies()).result;
    var promises = [];
    for (var cookie of data.cookies) {
      var url = (cookie.secure ? 'https://' : 'http://') + cookie.domain + cookie.path;
      promises.push(dp.Network.deleteCookies(cookie));
    }
    await Promise.all(promises);
  }

  async function setCookieViaFetch() {
    await dp.Runtime.evaluate({
        expression: `fetch('/inspector-protocol/network/resources/cookie.pl', { credentials: 'same-origin' })`,
        awaitPromise: true
    });
    await logCookies();
  }

  async function printCookieViaFetch() {
    await dp.Network.setCookie({url: 'http://127.0.0.1/', name: 'foo', value: 'bar1'});
    const body = (await dp.Runtime.evaluate({
        expression: `
            fetch('/inspector-protocol/network/resources/echo-headers.php?headers=HTTP_COOKIE',
                { credentials: 'same-origin' })
            .then(r => r.text())`,
        awaitPromise: true,
        returnByValue: true
    })).result.result.value;
    testRunner.log(`Cookies as seen on server: ${JSON.stringify(body)}`);
  }

  testRunner.log('Test started');
  testRunner.log('Enabling network');
  await dp.Network.enable();

  testRunner.runTestSuite([
    async function simpleCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar1'});
    },

    async function simpleCookieChange() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'second bar2'});
    },

    async function anotherSimpleCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo2', value: 'bar1'});
    },

    async function simpleCookieDelete() {
      await deleteCookie({url: 'http://127.0.0.1', name: 'foo'});
    },

    deleteAllCookies,

    async function sessionCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar4', expires: undefined});
    },

    deleteAllCookies,

    async function nonSessionCookieZeroAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar5', expires: 0});
    },

    deleteAllCookies,

    async function nonSessionCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar6', expires: Date.now() + 1000000});
    },

    deleteAllCookies,

    async function differentOriginCookieAdd() {
      // Will result in success but not show up
      await setCookie({url: 'http://example.com', name: 'foo', value: 'bar7'});
    },

    deleteAllCookies,

    async function invalidCookieAddDomain() {
      await setCookie({url: 'ht2tp://127.0.0.1', name: 'foo', value: 'bar8'});
    },

    deleteAllCookies,

    async function invalidCookieAddName() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo\0\r\na', value: 'bar9'});
    },

    deleteAllCookies,

    async function secureCookieAdd() {
      await setCookie({url: 'http://127.0.0.1', secure: true, name: 'foo', value: 'bar'});
    },

    deleteAllCookies,

    async function cookieAddHttpOnly() {
      await setCookie({url: 'http://127.0.0.1', httpOnly: true, name: 'foo', value: 'bar'});
    },

    deleteAllCookies,

    async function cookieAddSameSiteLax() {
      await setCookie({url: 'http://127.0.0.1', sameSite: 'Lax', name: 'foo', value: 'bar'});
    },

    deleteAllCookies,

    async function cookieAddSameSiteLax() {
      await setCookie({url: 'http://127.0.0.1', sameSite: 'Strict', name: 'foo', value: 'bar'});
    },

    deleteAllCookies,

    async function setCookiesBasic() {
      await setCookies([{name: 'cookie1', value: 'session', domain: 'localhost', path: '/', },
                        {name: 'cookie2', value: 'httpOnly', domain: 'localhost', path: '/', httpOnly: true },
                        {name: 'cookie3', value: 'secure', domain: 'localhost', path: '/', secure: true },
                        {name: 'cookie4', value: 'lax', domain: 'localhost', path: '/', sameSite: 'Lax' },
                        {name: 'cookie5', value: 'expires', domain: 'localhost', path: '/', expires: Date.now() + 1000 },
                        {name: 'cookie6', value: '.domain', domain: '.chromium.org', path: '/path' },
                        {name: 'cookie7', value: 'domain', domain: 'www.chromium.org', path: '/path' },
                        {name: 'cookie8', value: 'url-based', url: 'https://www.chromium.org/foo' }]);
    },

    deleteAllCookies,

    async function setCookiesWithInvalidCookie() {
      await setCookies([{url: '', name: 'foo', value: 'bar1'}]);
    },

    deleteAllCookies,

    async function deleteCookieByURL() {
      await setCookies([{name: 'cookie1', value: '.domain', url: 'http://www.chromium.org/path' },
                        {name: 'cookie2', value: '.domain', url: 'http://www.chromium.org/path', expires: Date.now() + 1000 }]);
      await deleteCookie({url: 'http://www.chromium.org/path', name: 'cookie1'});
    },

    deleteAllCookies,

    async function deleteCookieByDomain() {
      await setCookies([{name: 'cookie1', value: '.domain', domain: '.chromium.org', path: '/path' },
                        {name: 'cookie2', value: '.domain', domain: '.chromium.org', path: '/path', expires: Date.now() + 1000 }]);
      await deleteCookie({name: 'cookie1', domain: '.chromium.org'});
      await deleteCookie({name: 'cookie2', domain: '.chromium.org'});
    },

    deleteAllCookies,

    async function deleteCookieByDomainAndPath() {
      await setCookies([{name: 'cookie1', value: '.domain', domain: '.chromium.org', path: '/path' }]);
      await deleteCookie({name: 'cookie1', domain: '.chromium.org', path: '/foo'});
      await deleteCookie({name: 'cookie1', domain: '.chromium.org', path: '/path'});
    },

    deleteAllCookies,

    async function nonUnicodeCookie() {
      await setCookies([{name: 'cookie1', value: 'привет', domain: '.chromium.org', path: '/path' }]);
    },

    deleteAllCookies,

    setCookieViaFetch,

    deleteAllCookies,

    printCookieViaFetch,

    deleteAllCookies,
  ]);
})
