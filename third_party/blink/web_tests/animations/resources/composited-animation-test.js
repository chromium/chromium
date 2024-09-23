'use strict';

class CompositedAnimationTestCommon {
  constructor(composited) {
    this.composited = composited;
    this.tests = [];
    this.nextInstanceId = 1;
    this.errorCount = 0;

    this.createStyles();
    this.createStaticElements();
  }

  createStyles() {
    var styleSheet = document.createElement('style');
    styleSheet.textContent = `
    .item {
      width: 20px;
      height: 20px;
      position: relative;
      background: black;
      float: left;
    }
    .marker {
      width: 5px;
      height: 5px;
      display: inline-block;
      background: orange;
      margin: 15px;
    }`;

    document.head.appendChild(styleSheet);
  }

  createStaticElements() {
    this.error = document.createElement('span');
    this.error.style = 'color: red; font-family: monospace; font-size: 12px';
    // The error element must have some painted content in order to be
    // composited when animated in SPv2.
    this.error.innerText = '(no errors)';
    document.body.appendChild(this.error);

    this.wrapper = document.createElement('div');
    document.body.appendChild(this.wrapper);
  }

  createTestElements() {
    this.tests.forEach(test => {
      test.testWrapper = document.createElement('div');
      this.wrapper.appendChild(test.testWrapper);

      test.data.samples.forEach(sample => {
        var element = document.createElement('div');

        // Add marker custom style as inline style.
        // Do not create marker if empty string specified.
        if (test.data.markerStyle == null || test.data.markerStyle != '') {
          var content = document.createElement('div');
          content.classList.add('marker');
          content.style.cssText = test.data.markerStyle;
          element.appendChild(content);
        }

        element.classList.add('item');

        // Add custom style as inline style.
        var elementStyle = '';
        if (this.suiteStyle)
          elementStyle = this.suiteStyle;
        if (test.data.style)
          elementStyle += test.data.style;
        if (elementStyle)
          element.style.cssText = elementStyle;

        // New line.
        if (!test.testWrapper.hasChildNodes())
          element.style.clear = 'left';

        test.testWrapper.appendChild(element);

        test.instances.push({
          element: element,
          animation: null,
          id: this.nextInstanceId++
        });
      });
    });

    // Update all lifecycle phases to propagate all the objects to
    // the compositor and to clear all the dirty flags.
    if (window.internals)
      internals.forceCompositingUpdate(document);
  }

  startAnimations() {
    // We want to achieve desired accuracy for splines using a specific duration.
    // TODO(loyso): Duration mustn't affect cc/blink consistency.
    // Taken from cubic_bezier.cc:
    var kBezierEpsilon = 1e-7;
    // Reverse the blink::accuracyForDuration function to calculate duration
    // from epsilon:
    var duration = 1000 * 1.0 / (kBezierEpsilon * 200.0);

    this.tests.forEach(test => {
      if (test.instances.length != test.data.samples.length)
        this.reportError(test, `instances.length=${test.instances.length} != samples.length=${test.data.samples.length}`);

      for (var i = 0; i < test.instances.length; i++) {
        var sample = test.data.samples[i];
        var instance = test.instances[i];

        // Use negative animation delays to specify sampled time for each animation.
        instance.animation = instance.element.animate(test.data.keyframes, {
            duration: duration,
            iterations: Infinity,
            delay: -duration * sample.at,
            easing: test.data.easing
        });

        if (window.internals && !this.composited)
          internals.disableCompositedAnimation(instance.animation);
      }
    });

    if (window.internals)
      internals.pauseAnimations(0);
  }

  assertAnimationCompositedState() {
    this.tests.forEach(test => {
      test.instances.forEach(instance => {
        var composited = internals.isCompositedAnimation(instance.animation);
        if (composited != this.composited)
          this.reportError(test, `Animation ${composited ? 'is' : 'is not'} running on the compositor [id=${instance.id}].`);
      });
    });
  }

  reportError(test, message) {
    if (this.errorCount == 0)
      this.error.innerHTML = `${this.composited ? 'Tests:' : 'TestExpectations:'}<br>`;

    if (this.errorCount > 0)
        this.error.innerHTML += '<br>';
    this.error.innerHTML += `${test.name}: ${message} `;
    this.errorCount++;
  }

  waitForCompositor() {
    return this.error.animate({opacity: ['0', '1']}, 1).finished;
  }

  layoutAndPaint() {
    if (window.testRunner)
      testRunner.waitUntilDone();

    this.waitForCompositor().then(() => {
      requestAnimationFrame(() => {
        if (window.internals)
          this.assertAnimationCompositedState();
        if (window.testRunner)
          testRunner.notifyDone();
      });
    });
  }

  registerTestsData(testSuiteData) {
    this.suiteStyle = testSuiteData.style;
    for (var testName in testSuiteData.tests) {
      var testData = testSuiteData.tests[testName];
      this.tests.push({
        name: testName,
        data: testData,
        instances: []
      });
    }
  }

  run() {
    this.createTestElements();
    this.startAnimations();
    this.layoutAndPaint();
  }
}


class CompositedAnimationTest extends CompositedAnimationTestCommon {
  constructor() {
    var composited = true;
    super(composited)
  }
}


class CompositedAnimationTestExpected extends CompositedAnimationTestCommon {
  constructor() {
    var composited = false;
    super(composited)
  }
}


var runCompositedAnimationTests = function(testSuiteData) {
  var test = new CompositedAnimationTest();
  test.registerTestsData(testSuiteData);
  test.run();
}

var runCompositedAnimationTestExpectations = function(testSuiteData) {
  var test = new CompositedAnimationTestExpected();
  test.registerTestsData(testSuiteData);
  test.run();
}

var getLinearSamples = function(n, start, end) {
  var arr = [];
  var spread = end - start;
  for (var i = 0; i <= n; i++)
    arr.push(i * spread / n + start);
  return arr.map(t => { return {at: t} });
}

