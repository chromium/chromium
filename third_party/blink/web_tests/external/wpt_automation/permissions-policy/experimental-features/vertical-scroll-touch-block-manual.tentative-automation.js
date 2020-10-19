importAutomationScript('/permissions-policy/experimental-features/vertical-scroll.js');

function inject_scroll(direction) {
  return touchScroll(direction, window.innerWidth / 2, window.innerHeight / 2);
}

function inject_zoom(direction) {
  let width = window.innerWidth, height = window.innerHeight;
  return pinchZoom(direction, width / 4, height / 2, 3 * width / 4, height / 2);
}
