importAutomationScript('/permissions-policy/experimental-features/vertical-scroll.js');

function inject_input(direction) {
  return touchScroll(direction, window.innerWidth / 2, window.innerHeight / 2);
}
