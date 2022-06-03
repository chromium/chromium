// Disable compositor hit testing
document.addEventListener('touchstart', function() {});

window.addEventListener('load', function() {
  // Create any shadow DOM nodes requested by the test.
  var shadowTrees = document.querySelectorAll('[make-shadow-dom]');
  for (var i = 0; i < shadowTrees.length; i++) {
    var tree = shadowTrees[i];
    var host = tree.previousElementSibling;
    if (!host.hasAttribute('shadow-host')) {
      document.body.innerHTML = 'ERROR: make-shadow-dom node must follow a shadow-host node';
      return;
    }
    tree.parentElement.removeChild(tree);
    var shadowRoot = host.attachShadow({mode: 'open'});
    var style = document.createElement('style');
    style.innerText = ' .ta-none { -ms-touch-action: none; touch-action: none; }';
    shadowRoot.appendChild(style);
    shadowRoot.appendChild(tree);
  }
  // Generate the canvas element by script to ensure it's a replaced element.
  var canvasParent = document.getElementById('replaced-canvas');
  if (canvasParent) {
    var canvas = document.createElement('canvas');
    canvasParent.appendChild(canvas);
    canvas.className = 'ta-none';
    canvas.style = 'height: 0; margin-bottom: 50px; width: 300px; height: 150px;';
    canvas.setAttribute('expected-action', 'none');
    var context = canvas.getContext('2d');
    context.font = '13px serif';
    context.fillText('Touch action of replaced canvas should not be ignored', 0, 50);
  }
});

/*
 * Visualization of hit test locations for manual testing.
 * To be invoked manually (so it doesn't intefere with testing).
 */
function addMarker(x, y)
{
    const kMarkerSize = 6;
    var marker = document.createElement('div');
    marker.className = 'marker';
    marker.style.top = (y - kMarkerSize/2) + 'px';
    marker.style.left = (x - kMarkerSize/2) + 'px';
    document.body.appendChild(marker);
}

function addMarkers()
{
  var tests = document.querySelectorAll('[expected-action]');
  for (var i = 0; i < tests.length; i++) {
    var r = tests[i].getClientRects()[0];
    addMarker(r.left, r.top);
    addMarker(r.right - 1, r.bottom - 1);
    addMarker(r.left + r.width / 2, r.top + r.height / 2);
  }
}
