description("Setting a new font-family should reset the fallback list to empty before adding the new family.");

var wrapperDiv = document.createElement('div');
document.body.appendChild(wrapperDiv)

wrapperDiv.innerHTML =
'<div id="outerDiv" style="font-family: courier new, cursive;">' +
'should be Courier New' +
'<div id="timesDiv" style="font-family: foo;">should be Times (default font)</div>' +
'<div id="cursiveDiv" style="font-family: cursive;">should be cursive</div>' +
'</div>';

shouldBeEqualToString("window.getComputedStyle(outerDiv, null).fontFamily", "courier new, cursive");
shouldBeEqualToString("window.getComputedStyle(timesDiv, null).fontFamily", "foo");
shouldBeEqualToString("window.getComputedStyle(cursiveDiv, null).fontFamily", "cursive");

document.body.removeChild(wrapperDiv);
