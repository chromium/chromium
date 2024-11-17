function loadIframes(srcs) {
  var iframes = document.getElementsByTagName('iframe');
  for (var src of srcs) {
    for (var iframe of iframes) {
      loadFrame(iframe, src);
    }
  }
}

function hasAllowAttributeWithValue(frame, feature) {
  return frame.hasAttribute('allow') && frame.getAttribute('allow').includes(feature);
}
