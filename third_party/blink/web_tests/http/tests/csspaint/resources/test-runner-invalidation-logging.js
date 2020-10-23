// Test runner for the paint worklet to test invalidation behaviour.
//
// Registers an promise_test per test case, which:
//  - Creates an element.
//  - Invalidates the style of that element.
//  - [worklet] Worklet code logs if it got the invalidation.
//  - Asserts that it got the correct paint invalidation.
//
// Usage:
// testRunnerInvalidationLogging('background-image', [
//      { property: 'max-height', value: '100px' },
//      { property: 'color', prevValue: '#00F', value: 'blue', noInvalidation: true },
//      { property: 'margin-inline-start', invalidationProperty: 'margin-left', prevValue: 'calc(50px + 50px)', value: '100px', noInvalidation: true }
// ]);

function testRunnerInvalidationLogging(imageType, tests) {
    const keys = tests.map(function(obj) { return obj.property; });
    const workletCode = 'const properties = ' + JSON.stringify(keys) + ';\n' + `
        for (let i = 0; i < properties.length; i++) {
            registerPaint('paint-' + i, class {
                static get inputProperties() { return [properties[i]]; }
                constructor() { this.hasPainted= false; }
                paint(ctx, geom) {
                    ctx.fillStyle = this.hasPainted ? 'green' : 'blue';
                    ctx.fillRect(0, 0, geom.width, geom.height);
                    if (this.hasPainted) {
                        console.log('Successful invalidation for: ' + properties[i]);
                    }
                    this.hasPainted = true;
                }
            });
        }
    `;

    CSS.paintWorklet.addModule(URL.createObjectURL(new Blob([workletCode], {type: 'text/javascript'}))).then(function() {
        for (let i = 0; i < tests.length; i++) {
            tests[i].paintName = 'paint-' + i;
            registerTest(imageType, tests[i]);
        }
    });
}

function registerTest(imageType, test) {
    const testName = test.property + ': ' + (test.prevValue || '[inline not set]') + ' => ' + (test.invalidationProperty || test.property) + ': ' + (test.value || '[inline not set]');

    // We use a promise_test instead of a async_test as they run sequentially.
    promise_test(function() {
        return new Promise(function(resolve) {

            const msg = ['\n\nTest case:', testName];
            if (test.noInvalidation) {
                msg.push('The worklet console should log nothing');
            } else {
                msg.push('The worklet console should log: \'Successful invalidation for: ' + test.property + '\'');
            }
            console.log(msg.join('\n'));

            // Create the test div.
            const el = document.createElement('div');
            if (test.prevValue) el.style.setProperty(test.property, test.prevValue);
            el.style[imageType] = 'paint(' + test.paintName + ')';
            document.body.appendChild(el);

            runAfterLayoutAndPaint(function() {
                if (window.internals)
                    internals.startTrackingRepaints(document);

                // Keep the BCR for the paint invalidation assertion, and invalidate paint.
                const rect = el.getBoundingClientRect();
                if (test.invalidationProperty) {
                    el.style.setProperty(test.invalidationProperty, test.value);
                } else {
                    el.style.setProperty(test.property, test.value);
                }

                runAfterLayoutAndPaint(function() {
                    // Check that the we got the correct paint invalidation.
                    if (window.internals) {
                        const layers = JSON.parse(internals.layerTreeAsText(document, internals.LAYER_TREE_INCLUDES_INVALIDATIONS));
                        // Collect paint invalidations from all layers.
                        var invalidations = [];
                        layers.layers.forEach(layer => {
                          if (layer.invalidations)
                            invalidations.push.apply(invalidations, layer.invalidations);
                        });
                        var hasNoInvalidations = invalidations.length === 0;
                        assert_equals(hasNoInvalidations, !!test.noInvalidation);
                        if (!hasNoInvalidations) {
                            assert_equals(invalidations.length, 1, 'There should be only one invalidation.');
                            assert_array_equals(invalidations[0], [rect.left, rect.top, rect.width, rect.height],
                                                'The paint invalidation should cover the entire element.');
                        }
                        internals.stopTrackingRepaints(document);
                    }

                    // Cleanup.
                    document.body.removeChild(el);
                    resolve();
                });
            });

        });

    }, testName);
}
