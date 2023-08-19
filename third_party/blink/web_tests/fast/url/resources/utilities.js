// Start the bidding at 42 for no particular reason.
var lastID = 42;

function canonicalize(url)
{
  // Return `url` as is if `url` is an invalid URL. Otherwise, return the result
  // of the canonicalization of `url`.
  //
  // It would be more elegant to use the DOM here, but we use document.write()
  // so the tests run correctly in Firefox.
  var id = ++lastID;
  document.write("<a id='" + id + "' href='" + url + "'></a>");
  return document.getElementById(id).href;
}

function setBaseURL(url)
{
    // It would be more elegant to use the DOM here, but we chose document.write()
    // so the tests ran correctly in Firefox at the time we originally wrote them.

    // Remove any existing base elements.
    var existingBase = document.getElementsByTagName('base');
    while (existingBase.length) {
        var element = existingBase[0];
        element.parentNode.removeChild(element);
    }

    // Add a new base element.
    document.write('<base href="' + url + '">');
}

function segments(url)
{
  // It would be more elegant to use the DOM here, but we use document.write()
  // so the tests run correctly in Firefox.
  var id = ++lastID;
  document.write("<a id='" + id + "' href='" + url + "'></a>");
  var elmt = document.getElementById(id);
  return JSON.stringify([
    elmt.protocol,
    elmt.hostname,
    elmt.port,
    elmt.pathname,
    elmt.search,
    elmt.hash
  ]);
}
