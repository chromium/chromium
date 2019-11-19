// The functions in this file require gesture-util.js

function scroll(direction, source_device, precise_scrolling_delta) {
  return smoothScroll(800 /*pixels to scroll*/, x, y, source_device, direction,
      2000 /*pixels per sec*/, precise_scrolling_delta);
}

function scrollLeft(source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll("left", source_device, precise_scrolling_delta);
  return scroll("left", source_device);
}

function scrollUp(source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll("up", source_device, precise_scrolling_delta);
  return scroll("up", source_device);
}
function scrollUp(source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll("up", source_device, precise_scrolling_delta);
  return scroll("up", source_device);
}
function scrollRight(source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll("right", source_device, precise_scrolling_delta);
  return scroll("right", source_device);
}

function scrollDown(source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll("down", source_device, precise_scrolling_delta);
  return scroll("down", source_device);
}
