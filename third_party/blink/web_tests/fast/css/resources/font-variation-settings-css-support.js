var validWriteExpectations = [ "normal",
                               '"abcd" 1',
                               '"abcd" 0, "efgh" 1',
                               '"abcd" 0.3, "efgh" 1.4',
                               '"abcd" -1000, "efgh" -10000'];

var writeInvalidExpectations = ["none",
                                "'abc' 1",
                                "'abcde' 1",
                                "",
                                "'abc¡' 1",
                                "normal, 'abcd' 0",
                                "'abcd'",
                                "'abcd' 1 2",
                                "1 1",
                                "0 0",
                                "a a"];

setup({ explicit_done: true });

function styleAndComputedStyleReadback() {

    var testContainer = document.createElement("div");
    document.body.appendChild(testContainer);

    for (validValue of validWriteExpectations) {
        testContainer.style.fontVariationSettings = validValue;
        test(function() {
            assert_equals(testContainer.style.fontVariationSettings, validValue);
            assert_equals(getComputedStyle(testContainer).fontVariationSettings, validValue);
        }, 'font-variation-settings value ' + validValue + ' should be equal when reading it back from style and getComputedstyle.');
    }

    document.body.removeChild(testContainer);
}

function testInheritance() {
    var testContainer = document.createElement('div');
    var testContainerChild = document.createElement('div');
    testContainer.appendChild(testContainerChild);
    document.body.appendChild(testContainer);

    for (validValue of validWriteExpectations) {
        testContainer.style.fontVariationSettings = validValue;
        test(function() {
            assert_equals(testContainerChild.style.fontVariationSettings, '');
            assert_equals(getComputedStyle(testContainerChild).fontVariationSettings, validValue);
        }, 'font-variation-settings value ' + validValue + ' should be inherited to computed style of child.');
    }
    document.body.removeChild(testContainer);
}

function validWriteTests() {
    for (validValue of validWriteExpectations) {
        test(function() {
            assert_true(CSS.supports('font-variation-settings', validValue));
        }, 'Value ' + validValue + ' valid for property font-variation-settings');
    }
}

function invalidWriteTests() {
    for (invalidValue of writeInvalidExpectations) {
        test(function() {
            assert_false(CSS.supports('font-variation-settings', invalidValue));
        }, 'Value ' + invalidValue + ' invalid for property font-variation-settings');
    }
}

window.addEventListener('load', function() {
    validWriteTests();
    invalidWriteTests();
    styleAndComputedStyleReadback();
    testInheritance();
    done();
});
