description("Tests that the following classes are not manipulable by JavaScript (LegacyNoInterfaceObject).");

function shouldThrowReferenceError(expr)
{
    var e;
    try {
        eval(expr);
    } catch (_e) {
        e = _e;
    }

    var msg = expr + (e ? " threw exception " + e.name : " did not throw");
    if (e && e.name == "ReferenceError")
        testPassed(msg);
    else
        testFailed(msg);
}

function test(name)
{
    shouldBe('typeof ' + name, '"undefined"');
    shouldThrowReferenceError(name + '.prototype');
}

test('NavigatorUserMedia');
test('NavigatorUserMediaError');
test('NavigatorUserMediaSuccessCallback');
test('NavigatorUserMediaErrorCallback');

window.jsTestIsAsync = false;
