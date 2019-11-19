description("This test passes if it doesn't trigger an ASSERT - crbug.com/254203");

window.jsTestIsAsync = true;

var frameElem = document.getElementById('frame');
function onDone() {
  frameElem.removeEventListener('load', onDone);
  frameElem.src = 'about:blank';
  window.setTimeout(() => finishJSTest(), 0);
}
frameElem.addEventListener('load', onDone);
