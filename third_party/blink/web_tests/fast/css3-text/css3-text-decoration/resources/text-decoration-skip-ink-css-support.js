var validWriteExpectations = ['auto',
                               'none',
                               'all'];

var writeInvalidExpectations = ['none auto',
                                'non',
                                'abc def',
                                '',
                                'auto none',
                                'object'];

setup({ explicit_done: true });

function styleAndComputedStyleReadback() {

    var testContainer = document.createElement("div");
    document.body.appendChild(testContainer);

    for (validValue of validWriteExpectations) {
        testContainer.style.textDecorationSkipInk = validValue;
        test(function() {
            assert_equals(testContainer.style.textDecorationSkipInk, validValue);
            assert_equals(getComputedStyle(testContainer).textDecorationSkipInk, validValue);
        }, 'text-decoration-skip-ink value ' + validValue + ' should be equal when reading it back from style and getComputedstyle.');
    }

    document.body.removeChild(testContainer);
}

function testInheritance() {
    var testContainer = document.createElement('div');
    var testContainerChild = document.createElement('div');
    testContainer.appendChild(testContainerChild);
    document.body.appendChild(testContainer);

    for (validValue of validWriteExpectations) {
        testContainer.style.textDecorationSkipInk = validValue;
        test(function() {
            assert_equals(testContainerChild.style.textDecorationSkipInk, '');
            assert_equals(getComputedStyle(testContainerChild).textDecorationSkipInk, validValue);
        }, 'text-decoration-skip-ink value ' + validValue + ' should be inherited to computed style of child.');
    }
    document.body.removeChild(testContainer);
}

function validWriteTests() {
    for (validValue of validWriteExpectations) {
        test(function() {
            assert_true(CSS.supports('text-decoration-skip-ink', validValue));
        }, 'Value ' + validValue + ' valid for property text-decoration-skip-ink');
    }
}

function invalidWriteTests() {
    for (invalidValue of writeInvalidExpectations) {
        test(function() {
            assert_false(CSS.supports('text-decoration-skip-ink', invalidValue));
        }, 'Value ' + invalidValue + ' invalid for property text-decoration-skip-ink');
    }
}

window.addEventListener('load', function() {
    validWriteTests();
    invalidWriteTests();
    styleAndComputedStyleReadback();
    testInheritance();
    done();
});
