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
  // The '-webkit-' prefixed 'image-set' is expected to serialize to the same
  // value as standard 'image-set'.
  // https://drafts.csswg.org/css-images-4/#deprecated
  // "Implementations must accept -webkit-image-set() as a parse-time alias of
  // image-set(). (It’s a valid value, with identical arguments to image-set(),
  // and is turned into image-set() during parsing.)"
  const expected = `image-set(${rule})`;

  rule = `${isPrefixed ? '-webkit-' : ''}${expected}`;

  debug('');
  debug(`${description} : ${rule}`);

  const call = 'testComputedStyle(`background-image`, `' + rule + '`)';
  shouldBeEqualToString(call, expected);
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
