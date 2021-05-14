// The functions in this file require gesture-util.js

function scroll(
  pixelsToScroll, direction, source_device, precise_scrolling_delta
) {
  return smoothScroll(pixelsToScroll, x, y, source_device, direction,
    6000 /*pixels per sec*/, precise_scrolling_delta);
}

function scrollLeft(pixelsToScroll, source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll(
      pixelsToScroll, "left", source_device, precise_scrolling_delta
    );
  return scroll(pixelsToScroll, "left", source_device);
}

function scrollUp(pixelsToScroll, source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll(pixelsToScroll, "up", source_device, precise_scrolling_delta);
  return scroll(pixelsToScroll, "up", source_device);
}
function scrollUp(pixelsToScroll, source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll(pixelsToScroll, "up", source_device, precise_scrolling_delta);
  return scroll(pixelsToScroll, "up", source_device);
}
function scrollRight(pixelsToScroll, source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll(
      pixelsToScroll, "right", source_device, precise_scrolling_delta
    );
  return scroll(pixelsToScroll, "right", source_device);
}

function scrollDown(pixelsToScroll, source_device, precise_scrolling_delta) {
  if (source_device == GestureSourceType.MOUSE_INPUT)
    return scroll(
      pixelsToScroll, "down", source_device, precise_scrolling_delta
    );
  return scroll(pixelsToScroll, "down", source_device);
}
