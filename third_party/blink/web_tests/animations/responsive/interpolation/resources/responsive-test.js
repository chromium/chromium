// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/*
Exported functions:
assertCSSResponsive
assertSVGResponsive

Exported objects:
neutralKeyframe

Options format: {
  ?targetTag: <Target element tag name>,
  property: <Property/Attribute name>,
  ?getter(target): <Reads animated value from target>,
  from: <Value>,
  to: <Value>,
  configurations: [{
    state: {
      ?underlying: <Value>,
      ?inherited: <CSS Value>,
    },
    expect: [
      { at: <Float>, is: <Value> },
    ],
  }],
}

Description:
assertCSSResponsive() and assertSVGResponsive() take a property
specific interpolation and a list of style configurations with interpolation
expectations that apply to each configuration.
It starts the interpolation in every configuration, changes the
state to every other configuration (n * (n - 1) complexity) and asserts that
each destination configuration's expectations are met.
Each animation target can be assigned custom styles via the ".target" selector.
This test is designed to catch stale interpolation caches.
Set from/to to the exported neutralKeyframe object to use neutral keyframes.
*/

(function() {
'use strict';
var pendingResponsiveTests = [];
var htmlNamespace = 'http://www.w3.org/1999/xhtml';
var svgNamespace = 'http://www.w3.org/2000/svg';
var neutralKeyframe = {};

function assertCSSResponsive(options) {
  pendingResponsiveTests.push({
    options,
    bindings: {
      prefixProperty(property) {
        return toCamelCase(property);
      },
      createTargetContainer(container) {
        if (options.targetTag) {
          var svgRoot = createElement('svg', container, 'svg-root', svgNamespace);
          svgRoot.setAttribute('width', 0);
          svgRoot.setAttribute('height', 0);
          return svgRoot;
        }

        return createElement('div', container);
      },
      createTarget(container) {
        if (options.targetTag)
          return createElement(options.targetTag, container, 'target', svgNamespace);

        return createElement('div', container, 'target');
      },
      setValue(target, property, value) {
        test(function() {
          assert_true(CSS.supports(property, value), 'CSS.supports ' + property + ' ' + value);
        });
        target.style[property] = value;
      },
      getAnimatedValue(target, property) {
        return getComputedStyle(target)[property];
      },
    },
  });
}

function assertSVGResponsive(options) {
  pendingResponsiveTests.push({
    options,
    bindings: {
      prefixProperty(property) {
        return 'svg-' + property;
      },
      createTargetContainer(container) {
        var svgRoot = createElement('svg', container, 'svg-root', svgNamespace);
        svgRoot.setAttribute('width', 0);
        svgRoot.setAttribute('height', 0);
        return svgRoot;
      },
      createTarget(targetContainer) {
        console.assert(options.targetTag);
        return createElement(options.targetTag, targetContainer, 'target', svgNamespace);
      },
      setValue(target, property, value) {
        target.setAttribute(property, value);
      },
      getAnimatedValue(target, property) {
        return options.getter ? options.getter(target) : target[property].animVal;
      },
    },
  });
}

function createStateTransitions(configurations) {
  var stateTransitions = [];
  for (var i = 0; i < configurations.length; i++) {
    var beforeConfiguration = configurations[i];
    for (var j = 0; j < configurations.length; j++) {
      var afterConfiguration = configurations[j];
      if (j != i) {
        stateTransitions.push({
          before: beforeConfiguration,
          after: afterConfiguration,
        });
      }
    }
  }
  return stateTransitions;
}

function createElement(tag, container, className, namespace) {
  var element = document.createElementNS(namespace || htmlNamespace, tag);
  if (container) {
    container.appendChild(element);
  }
  if (className) {
    element.classList.add(className);
  }
  return element;
}

function createTargets(bindings, n, container) {
  var targets = [];
  for (var i = 0; i < n; i++) {
    targets.push(bindings.createTarget(container));
  }
  return targets;
}

function setState(bindings, targets, property, state) {
  for (var item in state) {
    switch (item) {
    case 'inherited':
      var parent = targets[0].parentElement;
      console.assert(targets.every(target => target.parentElement === parent));
      bindings.setValue(parent, property, state.inherited);
      break;
    case 'underlying':
      for (var target of targets) {
        bindings.setValue(target, property, state.underlying);
      }
      break;
    default:
      for (var target of targets) {
        bindings.setValue(target, item, state[item]);
      }
      break;
    }
  }
}

function isNeutralKeyframe(keyframe) {
  return keyframe === neutralKeyframe;
}

function keyframeText(keyframe) {
  return isNeutralKeyframe(keyframe) ? 'neutral' : `[${keyframe}]`;
}

function toCamelCase(property) {
  for (var i = property.length - 2; i > 0; --i) {
    if (property[i] === '-') {
      property = property.substring(0, i) + property[i + 1].toUpperCase() + property.substring(i + 2);
    }
  }
  return property;
}

function createKeyframes(prefixedProperty, from, to) {
  var keyframes = [];
  if (!isNeutralKeyframe(from)) {
    keyframes.push({
      offset: 0,
      [prefixedProperty]: from,
    });
  }
  if (!isNeutralKeyframe(to)) {
    keyframes.push({
      offset: 1,
      [prefixedProperty]: to,
    });
  }
  return keyframes;
}

function createPausedAnimations(targets, keyframes, fractions) {
  console.assert(targets.length == fractions.length);
  return targets.map((target, i) => {
    var fraction = fractions[i];
    console.assert(fraction >= 0 && fraction < 1);
    var animation = target.animate(keyframes, 1);
    animation.pause();
    animation.currentTime = fraction;
    return animation;
  });
}

function runPendingResponsiveTests() {
  return new Promise(resolve => {
    var stateTransitionTests = [];
    pendingResponsiveTests.forEach(responsiveTest => {
      var options = responsiveTest.options;
      var bindings = responsiveTest.bindings;
      var property = options.property;
      var prefixedProperty = bindings.prefixProperty(property);
      assert_true('from' in options);
      assert_true('to' in options);
      var from = options.from;
      var to = options.to;
      var keyframes = createKeyframes(prefixedProperty, from, to);

      var stateTransitions = createStateTransitions(options.configurations);
      stateTransitions.forEach(stateTransition => {
        var before = stateTransition.before;
        var after = stateTransition.after;
        var container = bindings.createTargetContainer(document.body);
        var targets = createTargets(bindings, after.expect.length, container);
        var expectationTargets = createTargets(bindings, after.expect.length, container);

        setState(bindings, targets, property, before.state);
        var animations = createPausedAnimations(targets, keyframes, after.expect.map(expectation => expectation.at));
        stateTransitionTests.push({
          applyStateTransition() {
            setState(bindings, targets, property, after.state);
          },
          assert() {
            for (var i = 0; i < targets.length; i++) {
              var target = targets[i];
              var expectation = after.expect[i];
              var expectationTarget = expectationTargets[i];
              bindings.setValue(expectationTarget, property, expectation.is);
              var actual = bindings.getAnimatedValue(target, property);
              test(() => {
                assert_equals(actual, bindings.getAnimatedValue(expectationTarget, property));
              }, `Animation on property <${prefixedProperty}> from ${keyframeText(from)} to ${keyframeText(to)} with ${JSON.stringify(before.state)} changed to ${JSON.stringify(after.state)} at (${expectation.at}) is [${expectation.is}]`);
            }
          },
        });
      });
    });

    requestAnimationFrame(() => {
      for (var stateTransitionTest of stateTransitionTests) {
        stateTransitionTest.applyStateTransition();
      }

      requestAnimationFrame(() => {
        for (var stateTransitionTest of stateTransitionTests) {
          stateTransitionTest.assert();
        }
        resolve();
      });
    });
  });
}

function loadScript(url) {
  return new Promise(resolve => {
    var script = document.createElement('script');
    script.src = url;
    script.onload = resolve;
    document.head.appendChild(script);
  });
}

loadScript('../../../resources/testharness.js').then(() => {
  return loadScript('../../../resources/testharnessreport.js');
}).then(() => {
  var asyncHandle = async_test('This test uses responsive-test.js.')
  runPendingResponsiveTests().then(() => {
    asyncHandle.done();
  });
});


window.assertCSSResponsive = assertCSSResponsive;
window.assertSVGResponsive = assertSVGResponsive;
window.neutralKeyframe = neutralKeyframe;

})();
