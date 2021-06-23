function createScroller(test) {
  var scroller = createDiv(test);
  scroller.innerHTML = "<div class='contents'></div>";
  scroller.classList.add('scroller');
  // Trigger layout run.
  scroller.scrollTop;
  return scroller;
}

function createScrollerWithStartAndEnd(test, orientationClass = 'vertical') {
  var scroller = createDiv(test);
  scroller.innerHTML =
    `<div class='contents'>
        <div id='start'></div>
        <div id='end'></div>
      </div>`;
  scroller.classList.add('scroller');
  scroller.classList.add(orientationClass);

  return scroller;
}

function createScrollTimeline(test, options) {
  options = options || {
    scrollSource: createScroller(test),
    timeRange: 1000
  }
  return new ScrollTimeline(options);
}

function createScrollTimelineWithOffsets(test, startOffset, endOffset) {
  return createScrollTimeline(test, {
    scrollSource: createScroller(test),
    orientation: "vertical",
    scrollOffsets: [startOffset, endOffset],
    timeRange: 1000
  });
}

function createScrollLinkedAnimation(test, timeline) {
  if (timeline === undefined)
    timeline = createScrollTimeline(test);
  const DURATION = 1000; // ms
  const KEYFRAMES = { opacity: [0, 1] };
  return new Animation(
    new KeyframeEffect(createDiv(test), KEYFRAMES, DURATION), timeline);
}
// These tests are used for the updated "progress based" animations
// which are only tested in a small number of files. In a later change
// the above functions that are used in all scroll timeline tests will be
// updated and these "progress based" duplicates can be removed. Needed work
// tracked by crbug.com/1216655

function createProgressBasedScrollTimeline(test, options) {
  options = options || {
    scrollSource: createScroller(test)
  }
  return new ScrollTimeline(options);
}

function createProgressBasedScrollTimelineWithOffsets(test, startOffset, endOffset) {
  return createScrollTimeline(test, {
    scrollSource: createScroller(test),
    orientation: "vertical",
    scrollOffsets: [startOffset, endOffset]
  });
}

function createProgressBasedScrollLinkedAnimation(test, timeline) {
  return createProgressBasedScrollLinkedAnimationWithTiming(test, /* duration in ms*/ 1000, timeline);
}

function createProgressBasedScrollLinkedAnimationWithTiming(test, timing, timeline) {
  if (timeline === undefined)
    timeline = createProgressBasedScrollTimeline(test);
  if (timing === undefined)
    timing = 1000; // ms
  const KEYFRAMES = { opacity: [0, 1] };
  return new Animation(
    new KeyframeEffect(createDiv(test), KEYFRAMES, timing), timeline);
}

function assert_approx_equals_or_null(actual, expected, tolerance, name){
  if (actual === null || expected === null){
    assert_equals(actual, expected, name);
  }
  else {
    assert_approx_equals(actual, expected, tolerance, name);
  }
}

// actual should be a CSSUnitValue and expected should be a double value 0-100
function assert_percent_css_unit_value_approx_equals(actual, expected, tolerance, name){
  assert_true(actual instanceof CSSUnitValue, "'actual' must be of type CSSUnitValue for \"" + name + "\"");
  assert_equals(typeof expected, "number", "'expected' should be a number (0-100) for \"" + name + "\"");
  assert_equals(actual.unit, "percent", "'actual' unit type must be 'percent' for \"" + name + "\"");
  assert_approx_equals(actual.value, expected, tolerance, name);
}

function assert_css_numberish_equals(actual, expected, name){
  assert_true(actual instanceof CSSUnitValue, "'actual' must be of type CSSNumberish for \"" + name + "\"");
  assert_true(expected instanceof CSSUnitValue, "'expected' must be of type CSSNumberish for \"" + name + "\"");
  assert_equals(actual.unit, expected.unit, "units do not match for  \"" + name + "\"");
  assert_equals(actual.value, expected.value, "values do not match for  \"" + name + "\"");
}