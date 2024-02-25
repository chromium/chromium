(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  // This needs to be https:// for the headers to work.
  const base = 'https://devtools.test:8443';

  // The page used enables the headers via an Accept-CH header.
  var {page, session, dp} = await testRunner.startURL(
      base + '/inspector-protocol/emulation/resources/set-accept-ch.php',
      'Test client hint behavior with setUserAgentOverride.');

  // Calling setUserAgentOverride w/o userAgentMetadata should first disable client hints
  testRunner.log('Testing without specifying userAgentMetadata');
  await dp.Emulation.setUserAgentOverride({userAgent: 'Custombrowser'});
  testRunner.log('navigator.userAgent == ' + await session.evaluate('navigator.userAgent'));
  testRunner.log('brands == ' + await session.evaluate('JSON.stringify(navigator.userAgentData.brands)'));
  testRunner.log('is mobile?' + await session.evaluate('navigator.userAgentData.mobile'));
  await printHeader('sec-ch-ua');
  await printHeader('sec-ch-ua-arch');
  await printHeader('sec-ch-ua-bitness');
  await printHeader('sec-ch-ua-full-version');
  await printHeader('sec-ch-ua-full-version-list');
  await printHeader('sec-ch-ua-platform');
  await printHeader('sec-ch-ua-platform-version');
  await printHeader('sec-ch-ua-mobile');
  await printHeader('sec-ch-ua-model');
  await printHeader('sec-ch-ua-wow64');

  // Now test with an override.
  testRunner.log('');
  testRunner.log('Testing with specifying userAgentMetadata');

  await dp.Emulation.setUserAgentOverride({
    userAgent: 'Ferrum Typewriter',
    userAgentMetadata: {
      brands: [{brand: 'Ferrum', version: '42.0'},
               {brand: 'Iron', version: '3'}],
      fullVersionList: [{brand: 'Ferrum', version: '42.0.3.14159'},
               {brand: 'Iron', version: '3.1.4.159'}],
      fullVersion: '42.0.3.14159',
      platform: 'Typewriter',
      platformVersion: '1950',
      architecture: 'Electromechanical',
      model: 'QWERTY',
      mobile: true,
      bitness: '64',
      wow64: false
    }
  });
  testRunner.log('navigator.userAgent == ' + await session.evaluate('navigator.userAgent'));
  testRunner.log('brands == ' + await session.evaluate('JSON.stringify(navigator.userAgentData.brands)'));
  testRunner.log('is mobile?' + await session.evaluate('navigator.userAgentData.mobile'));
  testRunner.log(await session.evaluateAsync(
      'navigator.userAgentData.getHighEntropyValues(' +
          '["architecture", "bitness", "fullVersionList", "platform", "platformVersion", "model", "uaFullVersion", "wow64"])'));
  await printHeader('sec-ch-ua');
  await printHeader('sec-ch-ua-arch');
  await printHeader('sec-ch-ua-bitness');
  await printHeader('sec-ch-ua-full-version');
  await printHeader('sec-ch-ua-full-version-list');
  await printHeader('sec-ch-ua-platform');
  await printHeader('sec-ch-ua-platform-version');
  await printHeader('sec-ch-ua-mobile');
  await printHeader('sec-ch-ua-model');
  await printHeader('sec-ch-ua-wow64');

  // Verifying that the low-entropy UA-CH are returned in getHighEntropyValues() by default
  testRunner.log('');
  testRunner.log('Testing with specifying getHighEntropyValues');
  testRunner.log(await session.evaluateAsync(
    'navigator.userAgentData.getHighEntropyValues(' +
        '["architecture"])'));
  await printHeader('sec-ch-ua');
  await printHeader('sec-ch-ua-arch');
  await printHeader('sec-ch-ua-platform');
  await printHeader('sec-ch-ua-mobile');

  // testing effect on navigation.
  testRunner.log('');
  testRunner.log('Testing effect on navigation');
  await session.navigate(base + '/inspector-protocol/emulation/resources/echo-headers.php');
  let navHeaders = await session.evaluate('document.documentElement.textContent');
  printHeaderFromList('sec-ch-ua', navHeaders);
  printHeaderFromList('sec-ch-ua-arch', navHeaders);
  printHeaderFromList('sec-ch-ua-bitness', navHeaders);
  printHeaderFromList('sec-ch-ua-full-version', navHeaders);
  printHeaderFromList('sec-ch-ua-full-version-list', navHeaders);
  printHeaderFromList('sec-ch-ua-platform', navHeaders);
  printHeaderFromList('sec-ch-ua-platform-version', navHeaders);
  printHeaderFromList('sec-ch-ua-mobile', navHeaders);
  printHeaderFromList('sec-ch-ua-model', navHeaders);
  printHeaderFromList('sec-ch-ua-wow64', navHeaders);

  // Tests to make sure that not passing in brand and fullVersion uses defaults
  testRunner.log('');
  testRunner.log('Testing defaulting of brand and fullVersion');

  await dp.Emulation.setUserAgentOverride({
    userAgent: 'Electric Typewriter',
    userAgentMetadata: {
      platform: 'Electric Typewriter',
      platformVersion: '1970',
      architecture: 'Electronic',
      model: 'With erase tape',
      mobile: true,
      bitness: '64',
      wow64: true
    }
  });
  testRunner.log('navigator.userAgent == ' + await session.evaluate('navigator.userAgent'));
  testRunner.log('brands == ' + await session.evaluate('JSON.stringify(navigator.userAgentData.brands)'));
  testRunner.log('is mobile?' + await session.evaluate('navigator.userAgentData.mobile'));
  testRunner.log(await session.evaluateAsync(
      'navigator.userAgentData.getHighEntropyValues(' +
          '["architecture", "bitness", "fullVersionList", "platform", "platformVersion", "model", "uaFullVersion", "wow64"])'));
  await printHeader('sec-ch-ua');
  await printHeader('sec-ch-ua-arch');
  await printHeader('sec-ch-ua-bitness');
  await printHeader('sec-ch-ua-full-version');
  await printHeader('sec-ch-ua-full-version-list');
  await printHeader('sec-ch-ua-platform');
  await printHeader('sec-ch-ua-platform-version');
  await printHeader('sec-ch-ua-mobile');
  await printHeader('sec-ch-ua-model');
  await printHeader('sec-ch-ua-wow64');

  function printHeaderFromList(name, headers) {
    let logged = false;
    for (const header of headers.split('\n')) {
      if (header.startsWith(name + ':')) {
        testRunner.log(header);
        logged = true;
      }
    }
    if (!logged) {
      testRunner.log('Missing header:' + name);
    }
  }

  async function printHeader(name) {
    const url = base + '/inspector-protocol/emulation/resources/echo-headers.php';
    const headers = await session.evaluateAsync(`fetch("${url}").then(r => r.text())`);
    printHeaderFromList(name, headers);
  }

  testRunner.completeTest();
})
