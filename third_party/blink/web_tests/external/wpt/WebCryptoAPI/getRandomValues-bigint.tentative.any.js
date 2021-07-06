// Tentative: see https://github.com/w3c/webcrypto/issues/255

const arrays = [
    'BigInt64Array',
    'BigUint64Array',
];

for (const array of arrays) {
    const ctor = globalThis[array];

    test(function() {
        assert_equals(self.crypto.getRandomValues(new ctor(8)).constructor,
                      ctor, "crypto.getRandomValues(new " + array + "(8))")
    }, "Integer array: " + array);

    test(function() {
        const maxlength = 65536 / ctor.BYTES_PER_ELEMENT;
        assert_throws_dom("QuotaExceededError", function() {
            self.crypto.getRandomValues(new ctor(maxlength + 1))
        }, "crypto.getRandomValues length over 65536")
    }, "Large length: " + array);

    test(function() {
        assert_true(self.crypto.getRandomValues(new ctor(0)).length == 0)
    }, "Null arrays: " + array);
}
