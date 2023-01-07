if( 'undefined' === typeof window){
    importScripts('../../../resources/js-test.js');
    importScripts('../../../resources/testharness.js');
    importScripts('../../../resources/testharnessreport.js');
}

function verifyContextLost(shouldBeLost, ctx) {
    // Verify context loss experimentally as well as contextLost
    ctx.fillStyle = '#0f0';
    ctx.fillRect(0, 0, 1, 1);
    contextLostTest = ctx.getImageData(0, 0, 1, 1).data[1] == 0;
    if (shouldBeLost) {
        assert_true(contextLostTest);
        assert_true(ctx.isContextLost());
    } else {
        assert_false(contextLostTest);
        assert_false(ctx.isContextLost());
    }
}

function contextLost(lostEventHasFired, ctx) {
    assert_false(lostEventHasFired, 'Graphics context lost event dispatched more than once.');
    lostEventHasFired = true;
    verifyContextLost(true, ctx);
}

function contextRestoredlostEventHasFired(lostEventHasFired, ctx) {
    assert_true(lostEventHasFired, 'Context restored event dispatched after context lost.');
    verifyContextLost(false, ctx);
}