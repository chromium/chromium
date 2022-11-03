(async function(testRunner) {

  var {page, session, dp} = await testRunner.startBlank(
      `Tests that cookies are set, updated and removed.`);

  async function logCookies(opt_data) {
    var data = opt_data || (await dp.Network.getAllCookies()).result;
    testRunner.log('Num of cookies ' + data.cookies.length);
    data.cookies.sort((a, b) => a.name.localeCompare(b.name));
    for (var cookie of data.cookies) {
      var suffix = ''
      if (cookie.partitionKeyOpaque)
        suffix += `, partitionKey: <opaque>`;
      else if (cookie.partitionKey)
        suffix += `, partitionKey: ${cookie.partitionKey}`;
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
    if (response.error)
      testRunner.log(`setCookie failed: ${response.error.message}`);
    await logCookies();
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

  function listenForSiteHasCookieInOtherPartition(event) {
    testRunner.log(
      'Site has cookie in other partition: '
          + event.params.siteHasCookieInOtherPartition);
  }

  async function getPartitionedCookies() {
    dp.Network.onRequestWillBeSentExtraInfo(
        listenForSiteHasCookieInOtherPartition);

    await page.navigate('https://devtools.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent.php');
    logCookies((await dp.Network.getCookies()).result);

    dp.Network.offRequestWillBeSentExtraInfo(
        listenForSiteHasCookieInOtherPartition);
  }

  testRunner.log('Test started');
  testRunner.log('Enabling network');
  await dp.Network.enable();

  testRunner.runTestSuite([
    deleteAllCookies,

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

    async function invalidCookieSourceScheme() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar10', sourceScheme: "SomeInvalidValue"});
    },

    deleteAllCookies,

    async function invalidCookieSourcePort() {
      await setCookie({url: 'http://127.0.0.1', name: 'foo', value: 'bar10', sourcePort: -1234});
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

    getPartitionedCookies,

    async function setPartitionedCookie() {
      await setCookie({url: 'https://devtools.test:8443', secure: true, name: '__Host-foo', value: 'bar', partitionKey: 'https://example.test:8443', sameSite: 'None'});
      await setCookie({url: 'https://example.test:8443', secure: true, name: '__Host-foo', value: 'bar', partitionKey: 'https://devtools.test:8443', sameSite: 'None'});
      await setCookie({url: 'https://example.test:8443', secure: true, name: '__Host-foo', value: 'bar', partitionKey: 'https://notinset.test:8443', sameSite: 'None'});
    },

    deleteAllCookies,
    logCookies,

    async function setPartitionedCookies() {
      await setCookies([
        {url: 'https://devtools.test:8443', secure: true, name: '__Host-foo', value: 'bar', partitionKey: 'https://example.test:8443', sameSite: 'None'},
        {url: 'https://example.test:8443', secure: true, name: '__Host-foo', value: 'bar', partitionKey: 'https://devtools.test:8443', sameSite: 'None'},
        {url: 'https://example.test:8443', secure: true, name: '__Host-foo', value: 'bar', partitionKey: 'https://notinset.test:8443', sameSite: 'None'}
      ]);
    },

    getPartitionedCookies,
    deleteAllCookies,

    async function getPartitionedCookieFromOpaqueOrigin() {
      dp.Network.onRequestWillBeSentExtraInfo(
        listenForSiteHasCookieInOtherPartition);

      await page.navigate('https://devtools.test:8443/inspector-protocol/resources/iframe-third-party-cookie-parent.php?opaque');
      logCookies((await dp.Network.getCookies()).result);

      dp.Network.offRequestWillBeSentExtraInfo(
        listenForSiteHasCookieInOtherPartition);
    },

    deleteAllCookies,
  ]);
})
