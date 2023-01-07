'use strict';

// Waits for a requestAnimationFrame callback in the next refresh driver tick.
function waitForNextFrame() {
  const timeAtStart = document.timeline.currentTime;
  return new Promise(resolve => {
   (function handleFrame() {
    if (timeAtStart === document.timeline.currentTime) {
      window.requestAnimationFrame(handleFrame);
    } else {
      resolve();
    }
  }());
  });
}

// creates div element, appends it to the document body and
// removes the created element during test cleanup
function createDiv(test, doc) {
  return createElement(test, 'div', doc);
}

// creates element of given tagName, appends it to the document body and
// removes the created element during test cleanup
// if tagName is null or undefined, returns div element
function createElement(test, tagName, doc) {
  if (!doc) {
    doc = document;
  }
  const element = doc.createElement(tagName || 'div');
  doc.body.appendChild(element);
  test.add_cleanup(() => {
    element.remove();
  });
  return element;
}

function createScroller(test) {
  var scroller = createDiv(test);
  scroller.innerHTML = "<div class='contents'></div>";
  scroller.classList.add('scroller');
  // Trigger layout run.
  scroller.scrollTop;
  return scroller;
}

function createScrollTimeline(test, options) {
  options = options || {
    source: createScroller(test)
  }
  return new ScrollTimeline(options);
}

function createScrollLinkedAnimation(test, timeline) {
  if (timeline === undefined)
    timeline = createScrollTimeline(test);
  const KEYFRAMES = { opacity: [0, 1] };
  return new Animation(
    new KeyframeEffect(createDiv(test), KEYFRAMES, {}), timeline);
}


// Scroll timeline times are measured as percentages and the error tolerance
// is set to half a pixel.  The |maxScroll| parameter is used to compute the
// maximum allowed error.
function assert_percents_approx_equal(actual, expected, maxScroll,
                                      description) {
  const tolerance = 0.5 / maxScroll * 100;
  assert_equals(actual.unit, "percent", `'actual' unit type must be ` +
      `'percent' for "${description}"`);
  assert_true(actual instanceof CSSUnitValue, `'actual' must be of type ` +
      `CSSNumberish for "${description}"`);
  if (expected instanceof CSSUnitValue){
    // Verify that when the expected in a CSSUnitValue, it is the correct unit
    // type
    assert_equals(expected.unit, "percent", `'expected' unit type must be ` +
        `'percent' for "${description}"`);
    assert_approx_equals(actual.value, expected.value, tolerance,
        `values do not match for "${description}"`);
  } else if (typeof expected, "number"){
    assert_approx_equals(actual.value, expected, tolerance,
        `values do not match for "${description}"`);
  }
}
