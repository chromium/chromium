<!DOCTYPE html>
<meta charset="UTF-8">
<style>
.parent {
  -webkit-mask-box-image-source: url(../resources/blue-100.png);
}
.target {
  display: inline-block;
  width: 100px;
  height: 100px;
  margin-bottom: 10px;
  background: black;
  /*-webkit-mask-box-image-slice: 25;*/
  -webkit-mask-box-image-source: url(../resources/stripes-100.png);
}
.expected {
  background: green;
  margin-right: 10px;
}
</style>
<body>
<script src="resources/interpolation-test.js"></script>
<script>
function assertCrossfadeInterpolation(options) {
  var fromComputed = options.fromComputed || options.from;
  assertInterpolation({
    property: '-webkit-mask-box-image-source',
    from: options.from,
    to: options.to,
  }, [
    {at: -0.3, is: fromComputed},
    {at: 0, is: fromComputed},
    {at: 0.3, is: '-webkit-cross-fade(' + fromComputed + ', ' + options.to + ', 0.3)'},
    {at: 0.5, is: '-webkit-cross-fade(' + fromComputed + ', ' + options.to + ', 0.5)'},
    {at: 0.6, is: '-webkit-cross-fade(' + fromComputed + ', ' + options.to + ', 0.6)'},
    {at: 1, is: options.to},
    {at: 1.5, is: options.to},
  ]);
}

// neutral
assertCrossfadeInterpolation({
  from: neutralKeyframe,
  fromComputed: 'url(../resources/stripes-100.png)',
  to: 'url(../resources/green-100.png)',
});

// initial
assertNoInterpolation({
  property: '-webkit-mask-box-image-source',
  from: 'initial',
  to: 'url(../resources/green-100.png)',
});

// inherit
assertCrossfadeInterpolation({
  from: 'inherit',
  fromComputed: 'url(../resources/blue-100.png)',
  to: 'url(../resources/green-100.png)',
});

// unset
assertNoInterpolation({
  property: '-webkit-mask-box-image-source',
  from: 'unset',
  to: 'url(../resources/stripes-100.png)',
});

// None to image
assertNoInterpolation({
  property: '-webkit-mask-box-image-source',
  from: 'none',
  to: 'url(../resources/stripes-100.png)',
});

// Image to image
assertCrossfadeInterpolation({
  property: '-webkit-mask-box-image-source',
  from: 'url(../resources/green-100.png)',
  to: 'url(../resources/stripes-100.png)',
});

// Image to gradient
assertCrossfadeInterpolation({
  property: '-webkit-mask-box-image-source',
  from: 'url(../resources/green-100.png)',
  to: 'linear-gradient(45deg, blue, orange)',
});

// Gradient to gradient
assertCrossfadeInterpolation({
  property: '-webkit-mask-box-image-source',
  from: 'linear-gradient(-45deg, red, yellow)',
  to: 'linear-gradient(45deg, blue, orange)',
});
</script>
</body>
