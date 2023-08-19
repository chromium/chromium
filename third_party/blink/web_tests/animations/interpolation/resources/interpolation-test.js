/*
 * Copyright (C) 2015 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * This script is intended to be used for constructing layout tests which
 * exercise the interpolation functionaltiy of the animation system.
 * Tests which run using this script should be portable across browsers.
 *
 * The following functions are exported:
 *  - assertInterpolation({property, from, to, [method]}, [{at: fraction, is: value}])
 *        Constructs a test case for each fraction that asserts the expected value
 *        equals the value produced by interpolation between from and to using
 *        CSS Animations, CSS Transitions and Web Animations. If the method option is
 *        specified then only that interpolation method will be used.
 *  - assertNoInterpolation({property, from, to, [method]})
 *        This works in the same way as assertInterpolation with expectations auto
 *        generated according to each interpolation method's handling of values
 *        that don't interpolate.
 *  - assertComposition(
 *          { property, underlying, [accumulateFrom], [accumulateTo],
 *            [addFrom], [addTo], [replaceFrom], [replaceTo] },
 *          [{at: fraction, is: value}])
 *        Similar to assertInterpolation() instead using only the Web Animations API
 *        to animate composite specified keyframes (accumulate, add or replace) on
 *        top of an underlying value.
 *        Exactly one of (accumulateFrom, addFrom, replaceFrom) must be specified.
 *        Exactly one of (accumulateTo, addTo, replaceTo) must be specified.
 *  - afterTest(callback)
 *        Calls callback after all the tests have executed.
 *
 * The following object is exported:
 *  - neutralKeyframe
 *        Can be used as the from/to value to use a neutral keyframe.
 */
'use strict';
(function() {
  var interpolationTests = [];
  var compositionTests = [];
  var cssAnimationsData = {
    sharedStyle: null,
    nextID: 0,
  };
  var webAnimationsEnabled = typeof Element.prototype.animate === 'function';
  var expectNoInterpolation = {};
  var afterTestHook = function() {};
  var neutralKeyframe = {};
  function isNeutralKeyframe(keyframe) {
    return keyframe === neutralKeyframe;
  }

  var cssAnimationsInterpolation = {
    name: 'CSS Animations',
    supportsProperty: function() {return true;},
    supportsValue: function() {return true;},
    setup: function() {},
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, 0.5);
    },
    interpolate: function(property, from, to, at, target) {
      var id = cssAnimationsData.nextID++;
      if (!cssAnimationsData.sharedStyle) {
        cssAnimationsData.sharedStyle = createElement(document.body, 'style');
      }
      cssAnimationsData.sharedStyle.textContent += '' +
        '@keyframes animation' + id + ' {' +
          (isNeutralKeyframe(from) ? '' : `from {${property}:${from};}`) +
          (isNeutralKeyframe(to) ? '' : `to {${property}:${to};}`) +
        '}';
      target.style.animationName = 'animation' + id;
      target.style.animationDuration = '2e10s';
      target.style.animationDelay = '-1e10s';
      target.style.animationTimingFunction = createEasing(at);
    },
    rebaseline: false,
  };

  var cssTransitionsInterpolation = {
    name: 'CSS Transitions',
    supportsProperty: function() {return true;},
    supportsValue: function() {return true;},
    setup: function(property, from, target) {
      target.style.setProperty(property, isNeutralKeyframe(from) ? '' : from);
    },
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, -Infinity);
    },
    interpolate: function(property, from, to, at, target) {
      target.style.transitionDuration = '2e10s';
      target.style.transitionDelay = '-1e10s';
      target.style.transitionTimingFunction = createEasing(at);
      target.style.transitionProperty = property;
      target.style.setProperty(property, isNeutralKeyframe(to) ? '' : to);
    },
    rebaseline: false,
  };

  var cssTransitionAllInterpolation = {
    name: 'CSS Transitions with transition: all',
    supportsProperty: function(property) {return property.indexOf('--') !== 0;},
    supportsValue: function() {return true;},
    setup: function(property, from, target) {
      target.style.setProperty(property, isNeutralKeyframe(from) ? '' : from);
    },
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, -Infinity);
    },
    interpolate: function(property, from, to, at, target) {
      target.style.transitionDuration = '2e10s';
      target.style.transitionDelay = '-1e10s';
      target.style.transitionTimingFunction = createEasing(at);
      target.style.transitionProperty = 'all';
      target.style.setProperty(property, isNeutralKeyframe(to) ? '' : to);
    },
    rebaseline: false,
  };

  var webAnimationsInterpolation = {
    name: 'Web Animations',
    supportsProperty: function(property) {return property.indexOf('-webkit-') !== 0;},
    supportsValue: function(value) {return value !== '';},
    setup: function() {},
    nonInterpolationExpectations: function(from, to) {
      return expectFlip(from, to, 0.5);
    },
    interpolate: function(property, from, to, at, target) {
      this.interpolateComposite(property, from, 'replace', to, 'replace', at, target);
    },
    interpolateComposite: function(property, from, fromComposite, to, toComposite, at, target) {
      // Convert standard properties to camelCase.
      if (!property.startsWith('--')) {
        for (var i = property.length - 2; i > 0; --i) {
          if (property[i] === '-') {
            property = property.substring(0, i) + property[i + 1].toUpperCase() + property.substring(i + 2);
          }
        }
        if (property === 'offset')
          property = 'cssOffset';
        else if (property === 'float')
          property = 'cssFloat';
      }
      var keyframes = [];
      if (!isNeutralKeyframe(from)) {
        keyframes.push({
          offset: 0,
          composite: fromComposite,
          [property]: from,
        });
      }
      if (!isNeutralKeyframe(to)) {
        keyframes.push({
          offset: 1,
          composite: toComposite,
          [property]: to,
        });
      }
      var animation = target.animate(keyframes, {
        fill: 'forwards',
        duration: 1,
        easing: createEasing(at),
      });
      animation.pause();
      animation.currentTime = 0.5;
    },
    rebaseline: false,
  };

  function expectFlip(from, to, flipAt) {
    return [-0.3, 0, 0.3, 0.5, 0.6, 1, 1.5].map(function(at) {
      return {
        at: at,
        is: at < flipAt ? from : to
      };
    });
  }

  // Constructs a timing function which produces 'y' at x = 0.5
  function createEasing(y) {
    // FIXME: if 'y' is > 0 and < 1 use a linear timing function and allow
    // 'x' to vary. Use a bezier only for values < 0 or > 1.
    if (y == 0) {
      return 'steps(1, end)';
    }
    if (y == 1) {
      return 'steps(1, start)';
    }
    if (y == 0.5) {
      return 'steps(2, end)';
    }
    // Approximate using a bezier.
    var b = (8 * y - 1) / 6;
    return 'cubic-bezier(0, ' + b + ', 1, ' + b + ')';
  }

  function createElement(parent, tag, text) {
    var element = document.createElement(tag || 'div');
    element.textContent = text || '';
    parent.appendChild(element);
    return element;
  }

  function loadScript(url) {
    return new Promise(function(resolve) {
      var script = document.createElement('script');
      script.src = url;
      script.onload = resolve;
      document.head.appendChild(script);
    });
  }

  function toCamelCase(property) {
    var i = property.length;
    while ((i = property.lastIndexOf('-', i - 1)) !== -1) {
      property = property.substring(0, i) + property[i + 1].toUpperCase() + property.substring(i + 2);
    }
    return property;
  }

  function createTargetContainer(parent, className) {
    var targetContainer = createElement(parent);
    targetContainer.classList.add('container');
    var template = document.querySelector('#target-template');
    if (template) {
      targetContainer.appendChild(template.content.cloneNode(true));
    }
    var target = targetContainer.querySelector('.target') || targetContainer;
    target.classList.add('target', className);
    target.parentElement.classList.add('parent');
    targetContainer.target = target;
    return targetContainer;
  }

  function roundNumbers(value) {
    return value.
        // Round numbers to two decimal places.
        replace(/-?\d*\.\d+(e-?\d+)?/g, function(n) {
          return (parseFloat(n).toFixed(2)).
              replace(/\.\d+/, function(m) {
                return m.replace(/0+$/, '');
              }).
              replace(/\.$/, '').
              replace(/^-0$/, '0');
        });
  }

  var anchor = document.createElement('a');
  function sanitizeUrls(value) {
    var matches = value.match(/url\("([^#][^\)]*)"\)/g);
    if (matches !== null) {
      for (var i = 0; i < matches.length; ++i) {
        var url = /url\("([^#][^\)]*)"\)/g.exec(matches[i])[1];
        anchor.href = url;
        anchor.pathname = '...' + anchor.pathname.substring(anchor.pathname.lastIndexOf('/'));
        value = value.replace(matches[i], 'url(' + anchor.href + ')');
      }
    }
    return value;
  }

  function normalizeValue(value) {
    return roundNumbers(sanitizeUrls(value)).
        // Place whitespace between tokens.
        replace(/([\w\d.]+|[^\s])/g, '$1 ').
        replace(/\s+/g, ' ');
  }

  function assertNoInterpolation(options) {
    assertInterpolation(options, expectNoInterpolation);
  }

  function assertInterpolation(options, expectations) {
    interpolationTests.push({options, expectations});
  }

  function assertComposition(options, expectations) {
    compositionTests.push({options, expectations});
  }

  function stringify(text) {
    if (!text.includes("'")) {
      return `'${text}'`;
    }
    return `"${text.replace('"', '\\"')}"`;
  }

  function keyframeText(keyframe) {
    return isNeutralKeyframe(keyframe) ? 'neutral' : `[${keyframe}]`;
  }

  function keyframeCode(keyframe) {
    return isNeutralKeyframe(keyframe) ? 'neutralKeyframe' : `${stringify(keyframe)}`;
  }

  function createInterpolationTestTargets(interpolationMethod, interpolationMethodContainer, interpolationTest, rebaselineContainer) {
    var property = interpolationTest.options.property;
    var from = interpolationTest.options.from;
    var to = interpolationTest.options.to;
    if ((interpolationTest.options.method && interpolationTest.options.method != interpolationMethod.name)
      || !interpolationMethod.supportsProperty(property)
      || !interpolationMethod.supportsValue(from)
      || !interpolationMethod.supportsValue(to)) {
      return;
    }
    if (interpolationMethod.rebaseline) {
      var rebaseline = createElement(rebaselineContainer, 'pre');
      rebaseline.appendChild(document.createTextNode(`\
assertInterpolation({
  property: '${property}',
  from: ${keyframeCode(from)},
  to: ${keyframeCode(to)},
}, [\n`));
      var rebaselineExpectation;
      rebaseline.appendChild(rebaselineExpectation = document.createTextNode(''));
      rebaseline.appendChild(document.createTextNode(']);\n\n'));
    }
    var testText = `${interpolationMethod.name}: property <${property}> from ${keyframeText(from)} to ${keyframeText(to)}`;
    var testContainer = createElement(interpolationMethodContainer, 'div', testText);
    createElement(testContainer, 'br');
    var expectations = interpolationTest.expectations;
    if (expectations === expectNoInterpolation) {
      expectations = interpolationMethod.nonInterpolationExpectations(from, to);
    }
    return expectations.map(function(expectation) {
      var actualTargetContainer = createTargetContainer(testContainer, 'actual');
      var expectedTargetContainer = createTargetContainer(testContainer, 'expected');
      if (!isNeutralKeyframe(expectation.is)) {
        expectedTargetContainer.target.style.setProperty(property, expectation.is);
      }
      var target = actualTargetContainer.target;
      interpolationMethod.setup(property, from, target);
      target.interpolate = function() {
        interpolationMethod.interpolate(property, from, to, expectation.at, target);
      };
      target.measure = function() {
        var actualValue = getComputedStyle(target).getPropertyValue(property);
        test(function() {
          assert_equals(
            normalizeValue(actualValue),
            normalizeValue(getComputedStyle(expectedTargetContainer.target).getPropertyValue(property)));
        }, `${testText} at (${expectation.at}) is [${sanitizeUrls(actualValue)}]`);
        if (rebaselineExpectation) {
          rebaselineExpectation.textContent += `  {at: ${expectation.at}, is: ${stringify(actualValue)}},\n`;
        }
      };
      return target;
    });
  }

  function createCompositionTestTargets(compositionContainer, compositionTest, rebaselineContainer) {
    var options = compositionTest.options;
    var property = options.property;
    var underlying = options.underlying;
    var from = options.accumulateFrom || options.addFrom || options.replaceFrom;
    var to = options.accumulateTo || options.addTo || options.replaceTo;
    var fromComposite = 'accumulateFrom' in options ? 'accumulate' : 'addFrom' in options ? 'add' : 'replace';
    var toComposite = 'accumulateTo' in options ? 'accumulate' : 'addTo' in options ? 'add' : 'replace';
    const invalidFrom = 'addFrom' in options === 'replaceFrom' in options
        && 'addFrom' in options === 'accumulateFrom' in options;
    const invalidTo = 'addTo' in options === 'replaceTo' in options
        && 'addTo' in options === 'accumulateTo' in options;
    if (invalidFrom || invalidTo) {
      test(function() {
        assert_false(invalidFrom, 'Exactly one of accumulateFrom, addFrom, or replaceFrom must be specified');
        assert_false(invalidTo, 'Exactly one of accumulateTo, addTo, or replaceTo must be specified');
      }, `Composition tests must have valid setup`);
    }
    validateTestInputs(property, from, to, underlying);

    if (webAnimationsInterpolation.rebaseline) {
      var rebaseline = createElement(rebaselineContainer, 'pre');
      rebaseline.appendChild(document.createTextNode(`\
assertComposition({
  property: '${property}',
  underlying: '${stringify(underlying)}',
  ${fromComposite}From: '${stringify(from)}',
  ${toComposite}To: '${stringify(to)}',
}, [\n`));
      var rebaselineExpectation;
      rebaseline.appendChild(rebaselineExpectation = document.createTextNode(''));
      rebaseline.appendChild(document.createTextNode(']);\n\n'));
    }
    var testText = `Compositing: property <${property}> underlying [${underlying}] from ${fromComposite} [${from}] to ${toComposite} [${to}]`;
    var testContainer = createElement(compositionContainer, 'div', testText);
    createElement(testContainer, 'br');
    return compositionTest.expectations.map(function(expectation) {
      var actualTargetContainer = createTargetContainer(testContainer, 'actual');
      var expectedTargetContainer = createTargetContainer(testContainer, 'expected');
      expectedTargetContainer.target.style.setProperty(property, expectation.is);
      var target = actualTargetContainer.target;
      target.style.setProperty(property, underlying);
      target.interpolate = function() {
        webAnimationsInterpolation.interpolateComposite(property, from, fromComposite, to, toComposite, expectation.at, target);
      };
      target.measure = function() {
        var actualValue = getComputedStyle(target).getPropertyValue(property);
        test(function() {
          assert_equals(
            normalizeValue(actualValue),
            normalizeValue(getComputedStyle(expectedTargetContainer.target).getPropertyValue(property)));
        }, `${testText} at (${expectation.at}) is [${sanitizeUrls(actualValue)}]`);
        if (rebaselineExpectation) {
          rebaselineExpectation.textContent += `  {at: ${expectation.at}, is: ${stringify(actualValue)}},\n`;
        }
      };
      return target;
    });
  }

  function validateTestInputs(property, from, to, underlying) {
    if (from && from !== neutralKeyframe && !CSS.supports(property, from)) {
        test(function() {
          assert_unreached('from value not supported');
        }, `${property} supports [${from}]`);
    }
    if (to && to !== neutralKeyframe && !CSS.supports(property, to)) {
        test(function() {
          assert_unreached('to value not supported');
        }, `${property} supports [${to}]`);
    }
    if (typeof underlying !== 'undefined' && !CSS.supports(property, underlying)) {
        test(function() {
          assert_unreached('underlying value not supported');
        }, `${property} supports [${underlying}]`);
    }
  }

  function createTestTargets(interpolationMethods, interpolationTests, compositionTests, container, rebaselineContainer) {
    var targets = [];
    for (var interpolationTest of interpolationTests) {
      validateTestInputs(interpolationTest.options.property, interpolationTest.options.from, interpolationTest.options.to);
    }
    for (var interpolationMethod of interpolationMethods) {
      var interpolationMethodContainer = createElement(container);
      for (var interpolationTest of interpolationTests) {
        [].push.apply(targets, createInterpolationTestTargets(interpolationMethod, interpolationMethodContainer, interpolationTest, rebaselineContainer));
      }
    }
    var compositionContainer = createElement(container);
    for (var compositionTest of compositionTests) {
      [].push.apply(targets, createCompositionTestTargets(compositionContainer, compositionTest, rebaselineContainer));
    }
    return targets;
  }

  function runTests() {
    var interpolationMethods = [
      cssTransitionsInterpolation,
      cssTransitionAllInterpolation,
      cssAnimationsInterpolation,
    ];
    if (webAnimationsEnabled) {
      interpolationMethods.push(webAnimationsInterpolation);
    }
    var rebaselineContainer = createElement(document.body);
    var container = createElement(document.body);
    var targets = createTestTargets(interpolationMethods, interpolationTests, compositionTests, container, rebaselineContainer);
    getComputedStyle(document.documentElement).left; // Force a style recalc for transitions.
    // Separate interpolation and measurement into different phases to avoid O(n^2) of the number of targets.
    for (var target of targets) {
      target.interpolate();
    }
    for (var target of targets) {
      target.measure();
    }
    if (window.testRunner) {
      container.remove();
    }
    afterTestHook();
  }

  function afterTest(f) {
    afterTestHook = f;
  }

  loadScript('../../resources/testharness.js').then(function() {
    return loadScript('../../resources/testharnessreport.js');
  }).then(function() {
    var asyncHandle = async_test('This test uses interpolation-test.js.')
    requestAnimationFrame(function() {
      runTests();
      asyncHandle.done()
    });
  });

  window.assertInterpolation = assertInterpolation;
  window.assertNoInterpolation = assertNoInterpolation;
  window.assertComposition = assertComposition;
  window.afterTest = afterTest;
  window.neutralKeyframe = neutralKeyframe;
})();
