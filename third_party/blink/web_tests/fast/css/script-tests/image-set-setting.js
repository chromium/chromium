description('Test the setting of the image-set functions.');

function testComputedStyle(property, rule) {
  var div = document.createElement('div');
  document.body.appendChild(div);
  div.setAttribute('style', property + ': ' + rule);

  var computedValue = div.style.backgroundImage;
  document.body.removeChild(div);

  return computedValue;
}

function testImageSetRule(description, rule, isPrefixed) {
  var rule = `${isPrefixed ? '-webkit-' : ''}image-set(${rule})`;

  debug('');
  debug(`${description} : ${rule}`);

  var call = 'testComputedStyle(`background-image`, `' + rule + '`)';
  shouldBeEqualToString(call, rule);
}

function testImageSetRules(description, rule) {
  // Test standard image-set
  testImageSetRule(description, rule, false);

  // Test '-webkit-' prefixed image-set
  testImageSetRule(description, rule, true);
}

testImageSetRules(
    'Single value for background-image', 'url("http://www.webkit.org/a") 1x');

testImageSetRules(
    'Multiple values for background-image',
    'url("http://www.webkit.org/a") 1x, url("http://www.webkit.org/b") 2x');

testImageSetRules(
    'Multiple values for background-image, out of order',
    'url("http://www.webkit.org/c") 3x, url("http://www.webkit.org/b") 2x, url("http://www.webkit.org/a") 1x');

testImageSetRules(
    'Duplicate values for background-image',
    'url("http://www.webkit.org/c") 1x, url("http://www.webkit.org/b") 2x, url("http://www.webkit.org/a") 1x');

testImageSetRules(
    'Fractional values for background-image',
    'url("http://www.webkit.org/c") 0.2x, url("http://www.webkit.org/b") 2.3x, url("http://www.webkit.org/a") 12.3456x');

// FIXME: We should also be testing the behavior of negative values somewhere, but it's currently
// broken.  http://wkb.ug/100132

successfullyParsed = true;
