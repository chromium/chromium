requirejs.config({
  baseUrl: '',
  paths: {
    'flight': 'components/flight'
  }
});

require(
  [
    'flight/lib/compose',
    'flight/lib/registry',
    'flight/lib/advice',
    'flight/lib/logger',
    'flight/tools/debug/debug'
  ],

  function(compose, registry, advice, withLogging, debug) {
    debug.enable(true);
    compose.mixin(registry, [advice.withAdvice, withLogging]);
    require(['app/boot/page'], function(initialize) {
      initialize();
    });
  }
);
