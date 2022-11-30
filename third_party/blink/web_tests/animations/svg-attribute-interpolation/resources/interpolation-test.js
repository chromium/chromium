/* Copyright 2015 The Chromium Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Exported function:
 *  - assertAttributeInterpolation({property, from, to, [fromComposite], [toComposite], [underlying]}, [{at: fraction, is: value}])
 *        Constructs a test case for each fraction that asserts the expected value
 *        equals the value produced by interpolation between from and to composited
 *        onto underlying by fromComposite and toComposite respectively using
 *        SMIL and Web Animations.
 *        Set from/to to the exported neutralKeyframe object to specify neutral keyframes.
 *        SMIL will only be tested with equal fromComposite and toComposite values.
*/
'use strict';
(() => {
  var interpolationTests = [];
  var neutralKeyframe = {};

  // Set to true to output rebaselined test expectations.
  var rebaselineTests = false;

  function isNeutralKeyframe(keyframe) {
    return keyframe === neutralKeyframe;
  }

  function createElement(tagName, container) {
    var element = document.createElement(tagName);
    if (container) {
      container.appendChild(element);
    }
    return element;
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

  function assertAttributeInterpolation(params, expectations) {
    interpolationTests.push({params, expectations});
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

  function normalizeValue(value) {
    return roundNumbers(value).
        // Place whitespace between tokens.
        replace(/([\w\d.]+|[^\s])/g, '$1 ').
        replace(/\s+/g, ' ');
  }

  function createTarget(container) {
    var targetContainer = createElement('div');
    var template = document.querySelector('#target-template');
    if (template) {
      targetContainer.appendChild(template.content.cloneNode(true));
      // Remove whitespace text nodes at start / end.
      while (targetContainer.firstChild.nodeType != Node.ELEMENT_NODE && !/\S/.test(targetContainer.firstChild.nodeValue)) {
        targetContainer.removeChild(targetContainer.firstChild);
      }
      while (targetContainer.lastChild.nodeType != Node.ELEMENT_NODE && !/\S/.test(targetContainer.lastChild.nodeValue)) {
        targetContainer.removeChild(targetContainer.lastChild);
      }
      // If the template contains just one element, use that rather than a wrapper div.
      if (targetContainer.children.length == 1 && targetContainer.childNodes.length == 1) {
        targetContainer = targetContainer.firstChild;
      }
      container.appendChild(targetContainer);
    }
    var target = targetContainer.querySelector('.target') || targetContainer;
    target.container = targetContainer;
    return target;
  }

  var anchor = createElement('a');
  function sanitizeUrls(value) {
    var matches = value.match(/url\([^\)]*\)/g);
    if (matches !== null) {
      for (var i = 0; i < matches.length; ++i) {
        var url = /url\(([^\)]*)\)/g.exec(matches[i])[1];
        anchor.href = url;
        anchor.pathname = '...' + anchor.pathname.substring(anchor.pathname.lastIndexOf('/'));
        value = value.replace(matches[i], 'url(' + anchor.href + ')');
      }
    }
    return value;
  }

  function serializeSVGLengthList(numberList) {
    var elements = [];
    for (var index = 0; index < numberList.numberOfItems; ++index)
      elements.push(numberList.getItem(index).value);
    return String(elements);
  }

  function serializeSVGNumberList(numberList) {
    return Array.from(numberList).map(number => number.value).join(', ');
  }

  function serializeSVGPointList(pointList) {
    var elements = [];
    for (var index = 0; index < pointList.numberOfItems; ++index) {
      var point = pointList.getItem(index);
      elements.push(point.x);
      elements.push(point.y);
    }
    return String(elements);
  }

  function serializeSVGPreserveAspectRatio(preserveAspectRatio) {
    return String([preserveAspectRatio.align, preserveAspectRatio.meetOrSlice]);
  }

  function serializeSVGRect(rect) {
    return [rect.x, rect.y, rect.width, rect.height].join(', ');
  }

  function serializeSVGTransformList(transformList) {
    var elements = [];
    for (var index = 0; index < transformList.numberOfItems; ++index) {
      var transform = transformList.getItem(index);
      elements.push(transform.type);
      elements.push(transform.angle);
      elements.push(transform.matrix.a);
      elements.push(transform.matrix.b);
      elements.push(transform.matrix.c);
      elements.push(transform.matrix.d);
      elements.push(transform.matrix.e);
      elements.push(transform.matrix.f);
    }
    return String(elements);
  }

  var svgNamespace = 'http://www.w3.org/2000/svg';
  var xlinkNamespace = 'http://www.w3.org/1999/xlink';

  var animatedNumberOptionalNumberAttributes = [
    'baseFrequency',
    'kernelUnitLength',
    'order',
    'radius',
    'stdDeviation',
  ];

  function namespacedAttributeName(attributeName) {
    if (attributeName === 'href')
      return 'xlink:href';
    return attributeName;
  }

  function getAttributeValue(element, attributeName) {
    if (animatedNumberOptionalNumberAttributes.includes(attributeName))
      return getAttributeValue(element, attributeName + 'X') + ', ' + getAttributeValue(element, attributeName + 'Y');

    // The attribute 'class' is exposed in IDL as 'className'
    if (attributeName === 'class')
      attributeName = 'className';

    // The attribute 'in' is exposed in IDL as 'in1'
    if (attributeName === 'in')
      attributeName = 'in1';

    // The attribute 'orient' is exposed in IDL as 'orientType' and 'orientAngle'
    if (attributeName === 'orient') {
      if (element['orientType'] && element['orientType'].animVal === SVGMarkerElement.SVG_MARKER_ORIENT_AUTO)
        return 'auto';
      attributeName = 'orientAngle';
    }

    var result = null;
    if (attributeName === 'd')
      result = getComputedStyle(element).getPropertyValue('d');
    else if (attributeName === 'points')
      result = element['animatedPoints'];
    else
      result = element[attributeName].animVal;

    if (result === null) {
      if (attributeName === 'pathLength')
        return '0';
      if (attributeName === 'preserveAlpha')
        return 'false';

      console.error('Unknown attribute, cannot get ' + attributeName);
      return null;
    }

    if (result instanceof SVGAngle || result instanceof SVGLength)
      result = result.value;
    else if (result instanceof SVGLengthList)
      result = serializeSVGLengthList(result);
    else if (result instanceof SVGNumberList)
      result = serializeSVGNumberList(result);
    else if (result instanceof SVGPointList)
      result = serializeSVGPointList(result);
    else if (result instanceof SVGPreserveAspectRatio)
      result = serializeSVGPreserveAspectRatio(result);
    else if (result instanceof SVGRect)
      result = serializeSVGRect(result);
    else if (result instanceof SVGTransformList)
      result = serializeSVGTransformList(result);

    if (typeof result !== 'string' && typeof result !== 'number' && typeof result !== 'boolean') {
      console.error('Attribute value has unexpected type: ' + result);
    }

    return String(result);
  }

  function setAttributeValue(element, attributeName, expectation) {
    if (!element[attributeName]
        && attributeName !== 'class'
        && attributeName !== 'd'
        && (attributeName !== 'in' || !element['in1'])
        && (attributeName !== 'orient' || !element['orientType'])
        && (animatedNumberOptionalNumberAttributes.indexOf(attributeName) === -1 || !element[attributeName + 'X'])) {
      console.error('Unknown attribute, cannot set ' + attributeName);
      return;
    }

    if (attributeName.toLowerCase().indexOf('transform') === -1) {
      var setElement = document.createElementNS(svgNamespace, 'set');
      setElement.setAttribute('attributeName', namespacedAttributeName(attributeName));
      setElement.setAttribute('attributeType', 'XML');
      setElement.setAttribute('to', expectation);
      element.appendChild(setElement);
    } else {
      element.setAttribute(attributeName, expectation);
    }
  }

  function createAnimateElement(attributeName, from, to, composite)
  {
    var animateElement;
    if (attributeName.toLowerCase().includes('transform')) {
      if (isNeutralKeyframe(from) || isNeutralKeyframe(to)) {
        return null;
      }
      from = from.split(')');
      to = to.split(')');
      // Discard empty string at end.
      from.pop();
      to.pop();

      // SMIL requires separate animateTransform elements for each transform in the list.
      if (from.length !== 1 || to.length !== 1) {
        return null;
      }

      from = from[0].split('(');
      to = to[0].split('(');
      if (from[0].trim() !== to[0].trim()) {
        return null;
      }

      animateElement = document.createElementNS(svgNamespace, 'animateTransform');
      animateElement.setAttribute('type', from[0].trim());
      animateElement.setAttribute('from', from[1]);
      animateElement.setAttribute('to', to[1]);
    } else {
      animateElement = document.createElementNS(svgNamespace, 'animate');
      animateElement.setAttribute('from', from);
      animateElement.setAttribute('to', to);
    }

    animateElement.setAttribute('attributeName', namespacedAttributeName(attributeName));
    animateElement.setAttribute('attributeType', 'XML');
    animateElement.setAttribute('begin', '0');
    animateElement.setAttribute('dur', '1');
    animateElement.setAttribute('fill', 'freeze');
    animateElement.setAttribute('additive', composite === 'add' ? 'sum' : composite);
    return animateElement;
  }

  function createTestTarget(method, description, container, params, expectation, rebaselineExpectation) {
    var target = createTarget(container);
    if (params.underlying) {
      target.setAttribute(params.property, params.underlying);
    }

    var expected = createTarget(container);
    setAttributeValue(expected, params.property, expectation.is);

    target.interpolate = function() {
      switch (method) {
      case 'SMIL':
        console.assert(params.fromComposite === params.toComposite);
        var animateElement = createAnimateElement(params.property, params.from, params.to, params.fromComposite);
        if (animateElement) {
          target.appendChild(animateElement);
          target.container.pauseAnimations();
          target.container.setCurrentTime(expectation.at);
        } else {
          target.container.remove();
          target.measure = function() {};
        }
        break;
      case 'Web Animations':
        // Replace 'transform' with 'svg-transform', etc. This avoids collisions with CSS properties or the Web Animations API (offset).
        var prefixedProperty = 'svg-' + params.property;
        var keyframes = [];
        if (!isNeutralKeyframe(params.from)) {
          keyframes.push({
            offset: 0,
            [prefixedProperty]: params.from,
            composite: params.fromComposite,
          });
        }
        if (!isNeutralKeyframe(params.to)) {
          keyframes.push({
            offset: 1,
            [prefixedProperty]: params.to,
            composite: params.toComposite,
          });
        }
        target.animate(keyframes, {
          fill: 'forwards',
          duration: 1,
          easing: createEasing(expectation.at),
          delay: -0.5,
          iterations: 0.5,
        });
        break;
      default:
        console.error('Unknown test method: ' + method);
      }
    };

    target.measure = function() {
      test(function() {
        var actualResult = getAttributeValue(target, params.property);
        if (rebaselineExpectation) {
          var roundResult = roundNumbers(actualResult);
          rebaselineExpectation.textContent += `  {at: ${expectation.at}, is: '${roundResult}'},\n`;
        }

        assert_equals(
          normalizeValue(actualResult),
          normalizeValue(getAttributeValue(expected, params.property)));
      }, `${method}: ${description} at (${expectation.at}) is [${expectation.is}]`);
    };

    return target;
  }

  function createTestTargets(interpolationTests, container, rebaselineContainer) {
    var targets = [];
    for (var interpolationTest of interpolationTests) {
      var params = interpolationTest.params;
      assert_true('property' in params);
      assert_true('from' in params);
      assert_true('to' in params);
      params.fromComposite = isNeutralKeyframe(params.from) ? 'add' : (params.fromComposite || 'replace');
      params.toComposite = isNeutralKeyframe(params.to) ? 'add' : (params.toComposite || 'replace');
      var underlyingText = params.underlying ? `with underlying [${params.underlying}] ` : '';
      var fromText = isNeutralKeyframe(params.from) ? 'neutral' : `${params.fromComposite} [${params.from}]`;
      var toText = isNeutralKeyframe(params.to) ? 'neutral' : `${params.toComposite} [${params.to}]`;
      var description = `Interpolate attribute <${params.property}> ${underlyingText}from ${fromText} to ${toText}`;

      if (rebaselineTests) {
        var rebaseline = createElement('pre', rebaselineContainer);

        var assertionCode =
          `assertAttributeInterpolation({\n` +
          `  property: '${params.property}',\n` +
          `  underlying: '${params.underlying}',\n`;


        if (isNeutralKeyframe(params.from)) {
          assertionCode += `  from: neutralKeyframe,\n`;
        } else {
          assertionCode +=
            `  from: '${params.from}',\n` +
            `  fromComposite: '${params.fromComposite}',\n`;
        }

        if (isNeutralKeyframe(params.to)) {
          assertionCode += `  to: neutralKeyframe,\n`;
        } else {
          assertionCode +=
            `  to: '${params.to}',\n` +
            `  toComposite: '${params.toComposite}',\n`;
        }

        assertionCode += `}, [\n`;

        rebaseline.appendChild(document.createTextNode(assertionCode));
        var rebaselineExpectation = document.createTextNode('');
        rebaseline.appendChild(rebaselineExpectation);
        rebaseline.appendChild(document.createTextNode(']);\n\n'));
      }

      for (var method of ['SMIL', 'Web Animations']) {
        if (method === 'SMIL' && params.fromComposite !== params.toComposite) {
          continue;
        }
        createElement('pre', container).textContent = `${method}: ${description}`;
        var smilContainer = createElement('div', container);
        for (var expectation of interpolationTest.expectations) {
          if (method === 'SMIL' && (expectation.at < 0 || expectation.at > 1)) {
            continue;
          }
          targets.push(createTestTarget(method, description, smilContainer, params, expectation, method === 'SMIL' ? null : rebaselineExpectation));
        }
      }
    }
    return targets;
  }

  function runTests() {
    return new Promise((resolve) => {
      var container = createElement('div', document.body);
      var rebaselineContainer = createElement('pre', document.body);
      var targets = createTestTargets(interpolationTests, container, rebaselineContainer);

      requestAnimationFrame(() => {
        for (var target of targets) {
          target.interpolate();
        }

        requestAnimationFrame(() => {
          for (var target of targets) {
            target.measure();
          }

          if (window.testRunner) {
            container.style.display = 'none';
          }

          resolve();
        });
      });
    });
  }

  function loadScript(url) {
    return new Promise(function(resolve) {
      var script = createElement('script', document.head);
      script.src = url;
      script.onload = resolve;
    });
  }

  loadScript('../../resources/testharness.js').then(() => {
    return loadScript('../../resources/testharnessreport.js');
  }).then(() => {
    var asyncHandle = async_test('This test uses interpolation-test.js.')
    requestAnimationFrame(() => {
      runTests().then(() => asyncHandle.done());
    });
  });

  window.assertAttributeInterpolation = assertAttributeInterpolation;
  window.neutralKeyframe = neutralKeyframe;
})();
