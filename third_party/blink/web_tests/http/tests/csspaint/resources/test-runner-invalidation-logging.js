// Test runner for the paint worklet to test invalidation behaviour.
//
// Registers a promise_test per test case, which:
//  - Creates an element.
//  - Invalidates the style of that element.
//  - Asserts that it got the correct paint invalidation.
//
// Usage:
// testRunnerInvalidation('background-image', [
//      { property: 'max-height', value: '100px' },
// ]);

function testRunnerInvalidation(imageType, tests) {
  const keys = tests.map(function(obj) {
    return obj.property;
  });
  const workletCode = 'const properties = ' + JSON.stringify(keys) + ';\n' +
      `
        for (let i = 0; i < properties.length; i++) {
            registerPaint('paint-' + i, class {
                static get inputProperties() { return [properties[i]]; }
                constructor() { this.hasPainted = false; }
                paint(ctx, geom) {
                    ctx.fillStyle = this.hasPainted ? 'green' : 'blue';
                    ctx.fillRect(0, 0, geom.width, geom.height);
                    this.hasPainted = true;
                }
            });
        }
    `;

  CSS.paintWorklet
      .addModule(URL.createObjectURL(
          new Blob([workletCode], {type: 'text/javascript'})))
      .then(function() {
        for (let i = 0; i < tests.length; i++) {
          tests[i].paintName = 'paint-' + i;
          registerTest(imageType, tests[i]);
        }
      });
}

function registerTest(imageType, test) {
  const testName = test.property + ': ' +
      (test.prevValue || '[inline not set]') + ' => ' +
      (test.invalidationProperty || test.property) + ': ' +
      (test.value || '[inline not set]');

  // We use a promise_test instead of an async_test as they run sequentially.
  promise_test(async function() {
    assert_true(!!window.internals, 'This test requires internals.');

    const el = document.createElement('div');
    if (test.prevValue)
      el.style.setProperty(test.property, test.prevValue);
    el.style[imageType] = 'paint(' + test.paintName + ')';
    document.body.appendChild(el);

    let trackingRepaints = false;
    try {
      await new Promise(resolve => runAfterLayoutAndPaint(resolve));
      internals.startTrackingRepaints(document);
      trackingRepaints = true;

      // Keep the BCR for the paint invalidation assertion, and invalidate
      // paint.
      const rect = el.getBoundingClientRect();
      if (test.invalidationProperty) {
        el.style.setProperty(test.invalidationProperty, test.value);
      } else {
        el.style.setProperty(test.property, test.value);
      }

      await new Promise(resolve => runAfterLayoutAndPaint(resolve));

      const layers = JSON.parse(internals.layerTreeAsText(
          document, internals.LAYER_TREE_INCLUDES_INVALIDATIONS));
      // Collect paint invalidations from all layers.
      const invalidations = [];
      layers.layers.forEach(layer => {
        if (layer.invalidations)
          invalidations.push.apply(invalidations, layer.invalidations);
      });
      const hasNoInvalidations = invalidations.length === 0;
      internals.stopTrackingRepaints(document);
      trackingRepaints = false;

      assert_equals(hasNoInvalidations, !!test.noInvalidation);
      if (!hasNoInvalidations) {
        assert_equals(invalidations.length, 1,
                      'There should be only one invalidation.');
        const expectedInvalidation = [
          rect.left, rect.top, rect.width, rect.height
        ].map(value => value * devicePixelRatio);
        assert_array_equals(
            invalidations[0], expectedInvalidation,
            'The paint invalidation should cover the entire element.');
      }
    } finally {
      if (trackingRepaints)
        internals.stopTrackingRepaints(document);
      el.remove();
    }
  }, testName);
}
