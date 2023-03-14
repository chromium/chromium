description('Test the parsing of the image-set functions.');

// These have to be global for the test helpers to see them.
var cssRule;

function testInvalidImageSet(description, rule, isPrefixed) {
  rule = `${isPrefixed ? '-webkit-' : ''}image-set(${rule}`;

  debug('');
  debug(`${description} : ${rule}`);

  var div = document.createElement('div');
  div.style.backgroundImage = rule;
  document.body.appendChild(div);

  cssRule = div.style.backgroundImage;
  shouldBeEmptyString('cssRule');

  document.body.removeChild(div);
}

function testInvalidImageSets(description, rule) {
  // Test standard image-set
  testInvalidImageSet(description, rule, false);

  // Test '-webkit-' prefixed image set
  testInvalidImageSet(description, rule, true);
}

testInvalidImageSets('Too many url parameters', 'url(#a #b)');

testInvalidImageSets('No x', 'url(\'#a\') 1');

testInvalidImageSets('No comma', 'url(\'#a\') 1x url(\'#b\') 2x');

testInvalidImageSets('Too many scale factor parameters', 'url(\'#a\') 1x 2x');

successfullyParsed = true;
