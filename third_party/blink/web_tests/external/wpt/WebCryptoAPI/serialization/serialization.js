function run_test(vectors) {
  function waitForEvent(obj, ev) {
    return new Promise((resolve) => {
      obj.addEventListener(ev, resolve, {once: true});
    });
  }

  function testCryptoKeySerialization(
      generateKeyAlgorithm, generateKeyUsages, exportFormat) {
    promise_test(async t => {
      var cryptoKey = await crypto.subtle.generateKey(
          generateKeyAlgorithm, true, generateKeyUsages);
      const keyExported =
          await crypto.subtle.exportKey(exportFormat, cryptoKey);

      const popup = window.open('resources/post-page.html');
      t.add_cleanup(() => popup.close());


      // Wait for window to load.
      await waitForEvent(popup, 'load');
      popup.postMessage({key: cryptoKey});
      // Wait to get key back via post.
      let evt = await waitForEvent(window, 'message');
      const newKeyExported =
          await crypto.subtle.exportKey(exportFormat, evt.data.key);
      assert_true(equalBuffers(keyExported, newKeyExported));
    }, 'serialization test ' + objectToString(generateKeyAlgorithm));
  };

  function testCryptoKeyPairSerialization(
      generateKeyAlgorithm, generateKeyUsages, publicExportFormat,
      privateExportFormat) {
    promise_test(async t => {
      var keyPair = await crypto.subtle.generateKey(
          generateKeyAlgorithm, true, generateKeyUsages);
      const publicKeyExported =
          await crypto.subtle.exportKey(publicExportFormat, keyPair.publicKey);
      const privateKeyExported = await crypto.subtle.exportKey(
          privateExportFormat, keyPair.privateKey);

      const popup = window.open('resources/post-page.html');
      t.add_cleanup(() => popup.close());

      // Wait for window to load.
      await waitForEvent(popup, 'load');
      popup.postMessage(
          {publicKey: keyPair.publicKey, privateKey: keyPair.privateKey});
      // Wait to get keys back via post.
      let evt = await waitForEvent(window, 'message');
      const newPublicKeyExported =
          await crypto.subtle.exportKey(publicExportFormat, evt.data.publicKey);
      assert_true(equalBuffers(publicKeyExported, newPublicKeyExported));
      const newPrivateKeyExported = await crypto.subtle.exportKey(
          privateExportFormat, evt.data.privateKey);
      assert_true(equalBuffers(privateKeyExported, newPrivateKeyExported));
    }, 'serialization test ' + objectToString(generateKeyAlgorithm));
  };

  vectors.forEach(function(vector) {
    if (vector.resultType === 'CryptoKey') {
      allAlgorithmSpecifiersFor(vector.name)
          .forEach(function(generateKeyAlgorithm) {
            testCryptoKeySerialization(
                generateKeyAlgorithm, vector.usages, vector.exportFormat);
          });
    } else {
      allAlgorithmSpecifiersFor(vector.name)
          .forEach(function(generateKeyAlgorithm) {
            testCryptoKeyPairSerialization(
                generateKeyAlgorithm, vector.usages, vector.publicFormat,
                vector.privateFormat);
          });
    }
  });
}
