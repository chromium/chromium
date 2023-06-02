// Karma configuration file
//
// For all available config options and default values, see:
// https://github.com/karma-runner/karma/blob/stable/lib/config.js#L54


// base path, that will be used to resolve files and exclude
basePath = '';

// list of files / patterns to load in the browser
files = [
  'components/es5-shim/es5-shim.js',
  'components/es5-shim/es5-sham.js',

  // frameworks
  JASMINE,
  JASMINE_ADAPTER,
  REQUIRE,
  REQUIRE_ADAPTER,

  // loaded without require
  'components/jquery/jquery.js',
  'components/jasmine-jquery/lib/jasmine-jquery.js',
  'components/jasmine-flight/lib/jasmine-flight.js',

  // loaded with require
  {pattern: 'components/flight/**/*.js', included: false},
  {pattern: 'components/mustache/**/*.js', included: false},
  {pattern: 'app/**/*.js', included: false},
  {pattern: 'test/fixtures/**/*.html', included: false},
  {pattern: 'test/spec/**/*.js', included: false},

  'test/test-main.js'
];

// list of files to exclude
exclude = [

];

// use dots reporter, as travis terminal does not support escaping sequences
reporters = [
  'dots'
];

// enable / disable watching file and executing tests whenever any file changes
// CLI --auto-watch --no-auto-watch
autoWatch = true;

// start these browsers
browsers = [
  'Chrome',
  'Firefox'
];

// auto run tests on start (when browsers are captured) and exit
// CLI --single-run --no-single-run
singleRun = false;
