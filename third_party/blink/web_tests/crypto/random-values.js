if (self.importScripts) {
    importScripts('../resources/js-test.js');
    importScripts('../resources/sab-polyfill.js');
}

description("Tests crypto.randomValues.");

if (!self.ArrayBuffer)
    debug("This test requres ArrayBuffers to run!");

shouldBe("'crypto' in self", "true");
shouldBe("'getRandomValues' in self.crypto", "true");

// Although the spec defines Crypto in terms of "RandomSource", it is NOT
// inheritance. The RandomSource interface should not be visible to
// javascript.
shouldBe("self.crypto.__proto__.hasOwnProperty('getRandomValues')", "true");

try {
    // NOTE: This test is flaky.  If we ran this test every second since the
    //       beginning of the universe, on average, it would have failed
    //       2^{-748} times.

    var reference = new Uint8Array(100);
    var sample = new Uint8Array(100);

    crypto.getRandomValues(reference);
    crypto.getRandomValues(sample);

    var matchingBytes = 0;

    for (var i = 0; i < reference.length; i++) {
        if (reference[i] == sample[i])
            matchingBytes++;
    }

    shouldBe("matchingBytes < 100", "true");
} catch(ex) {
    debug(ex);
}

shouldThrow("crypto.getRandomValues(new Uint8Array(new SharedArrayBuffer(100)))");

finishJSTest();
