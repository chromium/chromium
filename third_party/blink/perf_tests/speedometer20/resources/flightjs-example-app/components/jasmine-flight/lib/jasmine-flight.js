/**
 * Copyright 2013, Twitter Inc. and other contributors
 * Licensed under the MIT License
 */

(function (root) {
  'use strict';

  jasmine.flight = {};

  /**
   * Wrapper for describe. Load component before each test.
   *
   * @param componentPath
   * @param specDefinitions
   */

  root.describeComponent = function (componentPath, specDefinitions) {
    jasmine.getEnv().describeComponent(componentPath, specDefinitions);
  };

  jasmine.Env.prototype.describeComponent = function (componentPath, specDefinitions) {
    describe(componentPath, function () {
      beforeEach(function () {
        this.Component = this.component = this.$node = null;

        var requireCallback = function (registry, Component) {
          registry.reset();
          this.Component = Component;
        }.bind(this);

        require(['flight/lib/registry', componentPath], requireCallback);

        waitsFor(function () {
          return this.Component !== null;
        }.bind(this));
      });

      afterEach(function () {
        if (this.$node) {
          this.$node.remove();
          this.$node = null;
        }

        var requireCallback = function (defineComponent) {
          if (this.component) {
            this.component = null;
          }

          this.Component = null;
          defineComponent.teardownAll();
        }.bind(this);

        require(['flight/lib/component'], requireCallback);

        waitsFor(function () {
          return this.Component === null;
        }.bind(this));
      });

      specDefinitions.apply(this);
    });
  };

  /**
   * Wrapper for describe. Load mixin before each test.
   *
   * @param mixinPath
   * @param specDefinitions
   */

  root.describeMixin = function (mixinPath, specDefinitions) {
    jasmine.getEnv().describeMixin(mixinPath, specDefinitions);
  };

  jasmine.Env.prototype.describeMixin = function (mixinPath, specDefinitions) {
    describe(mixinPath, function () {
      beforeEach(function () {
        this.Component = this.component = this.$node = null;

        var requireCallback = function (registry, defineComponent, Mixin) {
          registry.reset();
          this.Component = defineComponent(function () {}, Mixin);
        }.bind(this);

        require(['flight/lib/registry', 'flight/lib/component', mixinPath], requireCallback);

        waitsFor(function () {
          return this.Component !== null;
        });
      });

      afterEach(function () {
        if (this.$node) {
          this.$node.remove();
          this.$node = null;
        }

        var requireCallback = function (defineComponent) {
          if (this.component) {
            this.component = null;
          }

          this.Component = null;
          defineComponent.teardownAll();
        }.bind(this);

        require(['flight/lib/component'], requireCallback);

        waitsFor(function () {
          return this.Component === null;
        }.bind(this));
      });

      specDefinitions.apply(this);
    });
  };

  /**
   * Wrapper for describe. Load module before each test.
   *
   * @param modulePath
   * @param specDefinitions
   */

  root.describeModule = function (modulePath, specDefinitions) {
    return jasmine.getEnv().describeModule(modulePath, specDefinitions);
  };

  jasmine.Env.prototype.describeModule = function (modulePath, specDefinitions) {
    describe(modulePath, function () {
      beforeEach(function () {
        this.module = null;

        var requireCallback = function (module) {
          this.module = module;
        }.bind(this);

        require([modulePath], requireCallback);

        waitsFor(function () {
          return this.module !== null;
        });
      });

      specDefinitions.apply(this);
    });
  };

  /**
   * Create root node and initialize component. Fixture should be html string
   * or jQuery object.
   *
   * @param fixture {String} (Optional)
   * @param options {Options} (Optional)
   */

  root.setupComponent = function (fixture, options) {
    jasmine.getEnv().currentSpec.setupComponent(fixture, options);
  };

  jasmine.Spec.prototype.setupComponent = function (fixture, options) {
    if (this.component) {
      this.component.teardown();
      this.$node.remove();
    }

    this.$node = $('<div class="component-root" />');
    $('body').append(this.$node);

    if (fixture instanceof jQuery || typeof fixture === 'string') {
      this.$node.append(fixture);
    } else {
      options = fixture;
      fixture = null;
    }

    options = options === undefined ? {} : options;

    this.component = new this.Component(this.$node, options);
  };


  (function (namespace) {
    var eventsData = {
      spiedEvents: {},
      handlers:    []
    };

    namespace.formatElement = function ($element) {
      var limit = 200;
      var output = '';

      if ($element instanceof jQuery) {
        output = jasmine.JQuery.elementToString($element);
        if (output.length > limit) {
          output = output.slice(0, 200) + '...';
        }
      } else {
        //$element should always be a jQuery object
        output = 'element is not a jQuery object';
      }

      return output;
    };

    namespace.compareColors = function (color1, color2) {
      if (color1.charAt(0) === color2.charAt(0)) {
        return color1 === color2;
      } else {
        return namespace.hex2rgb(color1) === namespace.hex2rgb(color2);
      }
    };

    namespace.hex2rgb = function (colorString) {
      if (colorString.charAt(0) !== '#') return colorString;
      // note: hexStr should be #rrggbb
      var hex = parseInt(colorString.substring(1), 16);
      var r = (hex & 0xff0000) >> 16;
      var g = (hex & 0x00ff00) >> 8;
      var b = hex & 0x0000ff;
      return 'rgb(' + r + ', ' + g + ', ' + b + ')';
    };

    namespace.events = {
      spyOn: function (selector, eventName) {
        eventsData.spiedEvents[[selector, eventName]] = {
          callCount: 0,
          calls: [],
          mostRecentCall: {},
          name: eventName
        };

        var handler = function (e, data) {
          var call = {
            event: e,
            args: jasmine.util.argsToArray(arguments),
            data: data
          };
          eventsData.spiedEvents[[selector, eventName]].callCount++;
          eventsData.spiedEvents[[selector, eventName]].calls.push(call);
          eventsData.spiedEvents[[selector, eventName]].mostRecentCall = call;
        };

        jQuery(selector).on(eventName, handler);
        eventsData.handlers.push(handler);
        return eventsData.spiedEvents[[selector, eventName]];
      },

      eventArgs: function (selector, eventName, expectedArg) {
        var actualArgs = eventsData.spiedEvents[[selector, eventName]].mostRecentCall.args;

        if (!actualArgs) {
          throw 'No event spy found on ' + eventName + '. Try adding a call to spyOnEvent or make sure that the selector the event is triggered on and the selector being spied on are correct.';
        }

        // remove extra event metadata if it is not tested for
        if ((actualArgs.length === 2) && typeof actualArgs[1] === 'object' &&
          expectedArg && !expectedArg.scribeContext && !expectedArg.sourceEventData && !expectedArg.scribeData) {
          actualArgs[1] = $.extend({}, actualArgs[1]);
          delete actualArgs[1].sourceEventData;
          delete actualArgs[1].scribeContext;
          delete actualArgs[1].scribeData;
        }

        return actualArgs;
      },

      wasTriggered: function (selector, event) {
        var spiedEvent = eventsData.spiedEvents[[selector, event]];
        return spiedEvent && spiedEvent.callCount > 0;
      },

      wasTriggeredWith: function (selector, eventName, expectedArg, env) {
        var actualArgs = jasmine.flight.events.eventArgs(selector, eventName, expectedArg);
        return actualArgs && env.contains_(actualArgs, expectedArg);
      },

      wasTriggeredWithData: function (selector, eventName, expectedArg, env) {
        var actualArgs = jasmine.flight.events.eventArgs(selector, eventName, expectedArg);
        var valid;

        if (actualArgs) {
          valid = false;
          for (var i = 0; i < actualArgs.length; i++) {
            if (jasmine.flight.validateHash(expectedArg, actualArgs[i])) {
              return true;
            }
          }
          return valid;
        }

        return false;
      },

      cleanUp: function () {
        eventsData.spiedEvents = {};
        eventsData.handlers    = [];
      }
    };

    namespace.validateHash = function (a, b, intersection) {
      var validHash;
      for (var field in a) {
        if ((typeof a[field] === 'object') && (typeof b[field] === 'object')) {
          validHash = jasmine.flight.validateHash(a[field], b[field]);
        } else if (intersection && (typeof a[field] === 'undefined' || typeof b[field] === 'undefined')) {
          validHash = true;
        } else {
          validHash = (a[field] === b[field]);
        }
        if (!validHash) {
          break;
        }
      }
      return validHash;
    };
  })(jasmine.flight);

  beforeEach(function () {
    this.addMatchers({
      toHaveBeenTriggeredOn: function () {
        var selector = arguments[0];
        var eventName = typeof this.actual === 'string' ? this.actual : this.actual.name;
        var wasTriggered = jasmine.flight.events.wasTriggered(selector, eventName);

        this.message = function () {
          var $pp = function (obj) {
            var description;
            var attr;

            if (!(obj instanceof jQuery)) {
              obj = $(obj);
            }

            description = [
              obj.get(0).nodeName
            ];

            attr = obj.get(0).attributes || [];

            for (var x = 0; x < attr.length; x++) {
              description.push(attr[x].name + '="' + attr[x].value + '"');
            }

            return '<' + description.join(' ') + '>';
          };

          if (wasTriggered) {
            return [
              '<div class="value-mismatch">Expected event ' + eventName + ' to have been triggered on' + selector,
              '<div class="value-mismatch">Expected event ' + eventName + ' not to have been triggered on' + selector
            ];
          } else {
            return [
              'Expected event ' + eventName + ' to have been triggered on ' + $pp(selector),
              'Expected event ' + eventName + ' not to have been triggered on ' + $pp(selector)
            ];
          }
        };

        return wasTriggered;
      },

      toHaveBeenTriggeredOnAndWith: function () {
        var selector = arguments[0];
        var expectedArg = arguments[1];
        var exactMatch = !arguments[2];
        var wasTriggered = jasmine.flight.events.wasTriggered(selector, this.actual);

        this.message = function () {
          var $pp = function (obj) {
            var description;
            var attr;

            if (!(obj instanceof jQuery)) {
              obj = $(obj);
            }

            description = [
              obj.get(0).nodeName
            ];

            attr = obj.get(0).attributes || [];

            for (var x = 0; x < attr.length; x++) {
              description.push(attr[x].name + '="' + attr[x].value + '"');
            }

            return '<' + description.join(' ') + '>';
          };

          if (wasTriggered) {
            var actualArg = jasmine.flight.events.eventArgs(selector, this.actual, expectedArg)[1];
            return [
              '<div class="value-mismatch">Expected event ' + this.actual.name + ' to have been triggered on' + selector,
              '<div class="value-mismatch">Expected event ' + this.actual.name + ' not to have been triggered on' + selector
            ];
          } else {
            return [
              'Expected event ' + this.actual.name + ' to have been triggered on ' + $pp(selector),
              'Expected event ' + this.actual.name + ' not to have been triggered on ' + $pp(selector)
            ];
          }
        };

        if (!wasTriggered) {
          return false;
        }

        if (exactMatch) {
          return jasmine.flight.events.wasTriggeredWith(selector, this.actual, expectedArg, this.env);
        } else {
          return jasmine.flight.events.wasTriggeredWithData(selector, this.actual, expectedArg, this.env);
        }
      },

      toHaveCss: function (prop, val) {
        var result;
        if (val instanceof RegExp) {
          result = val.test(this.actual.css(prop));
        } else if (prop.match(/color/)) {
          //IE returns colors as hex strings; other browsers return rgb(r, g, b) strings
          result = jasmine.flight.compareColors(this.actual.css(prop), val);
        } else {
          result = this.actual.css(prop) === val;
          //sometimes .css() returns strings when it should return numbers
          if (!result && typeof val === 'number') {
            result = parseFloat(this.actual.css(prop), 10) === val;
          }
        }

        this.actual = jasmine.flight.formatElement(this.actual);
        return result;
      }
    });
  });

  root.spyOnEvent = function (selector, eventName) {
    jasmine.JQuery.events.spyOn(selector, eventName);
    return jasmine.flight.events.spyOn(selector, eventName);
  };

}(this));
