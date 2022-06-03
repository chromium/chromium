importAutomationScript('/permissions-policy/experimental-features/vertical-scroll.js');

function inject_wheel_scroll(direction) {
  return wheelScroll(direction, window.innerWidth / 2, window.innerHeight / 2);
}
