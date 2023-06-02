"use strict";

/* jshint ignore:start */



/* jshint ignore:end */

define('todomvc/app', ['exports', 'ember', 'todomvc/resolver', 'ember-load-initializers', 'todomvc/config/environment'], function (exports, _ember, _todomvcResolver, _emberLoadInitializers, _todomvcConfigEnvironment) {

    var App = undefined;

    _ember['default'].MODEL_FACTORY_INJECTIONS = true;

    App = _ember['default'].Application.extend({
        modulePrefix: _todomvcConfigEnvironment['default'].modulePrefix,
        podModulePrefix: _todomvcConfigEnvironment['default'].podModulePrefix,
        Resolver: _todomvcResolver['default']
    });

    (0, _emberLoadInitializers['default'])(App, _todomvcConfigEnvironment['default'].modulePrefix);

    exports['default'] = App;
});
define('todomvc/components/app-version', ['exports', 'ember-cli-app-version/components/app-version', 'todomvc/config/environment'], function (exports, _emberCliAppVersionComponentsAppVersion, _todomvcConfigEnvironment) {

  var name = _todomvcConfigEnvironment['default'].APP.name;
  var version = _todomvcConfigEnvironment['default'].APP.version;

  exports['default'] = _emberCliAppVersionComponentsAppVersion['default'].extend({
    version: version,
    name: name
  });
});
define('todomvc/components/todo-item', ['exports', 'ember'], function (exports, _ember) {
    exports['default'] = _ember['default'].Component.extend({
        repo: _ember['default'].inject.service(),
        tagName: 'li',
        editing: false,
        classNameBindings: ['todo.completed', 'editing'],

        actions: {
            startEditing: function startEditing() {
                this.get('onStartEdit')();
                this.set('editing', true);
                _ember['default'].run.scheduleOnce('afterRender', this, 'focusInput');
            },

            doneEditing: function doneEditing(todoTitle) {
                if (!this.get('editing')) {
                    return;
                }
                if (_ember['default'].isBlank(todoTitle)) {
                    this.send('removeTodo');
                } else {
                    this.set('todo.title', todoTitle.trim());
                    this.set('editing', false);
                    this.get('onEndEdit')();
                }
            },

            handleKeydown: function handleKeydown(e) {
                if (e.keyCode === 13) {
                    e.target.blur();
                } else if (e.keyCode === 27) {
                    this.set('editing', false);
                }
            },

            toggleCompleted: function toggleCompleted(e) {
                var todo = this.get('todo');
                _ember['default'].set(todo, 'completed', e.target.checked);
                this.get('repo').persist();
            },

            removeTodo: function removeTodo() {
                this.get('repo')['delete'](this.get('todo'));
            }
        },

        focusInput: function focusInput() {
            this.element.querySelector('input.edit').focus();
        }
    });
});
define('todomvc/components/todo-list', ['exports', 'ember'], function (exports, _ember) {
    exports['default'] = _ember['default'].Component.extend({
        repo: _ember['default'].inject.service(),
        tagName: 'section',
        elementId: 'main',
        canToggle: true,
        allCompleted: _ember['default'].computed('todos.@each.completed', function () {
            return this.get('todos').isEvery('completed');
        }),

        actions: {
            enableToggle: function enableToggle() {
                this.set('canToggle', true);
            },

            disableToggle: function disableToggle() {
                this.set('canToggle', false);
            },

            toggleAll: function toggleAll() {
                var allCompleted = this.get('allCompleted');
                this.get('todos').forEach(function (todo) {
                    return _ember['default'].set(todo, 'completed', !allCompleted);
                });
                this.get('repo').persist();
            }
        }
    });
});
define('todomvc/controllers/active', ['exports', 'ember'], function (exports, _ember) {
    exports['default'] = _ember['default'].Controller.extend({
        todos: _ember['default'].computed.filterBy('model', 'completed', false)
    });
});
define('todomvc/controllers/application', ['exports', 'ember'], function (exports, _ember) {
    exports['default'] = _ember['default'].Controller.extend({
        repo: _ember['default'].inject.service(),
        remaining: _ember['default'].computed.filterBy('model', 'completed', false),
        completed: _ember['default'].computed.filterBy('model', 'completed'),
        actions: {
            createTodo: function createTodo(e) {
                if (e.keyCode === 13 && !_ember['default'].isBlank(e.target.value)) {
                    this.get('repo').add({ title: e.target.value.trim(), completed: false });
                    e.target.value = '';
                }
            },

            clearCompleted: function clearCompleted() {
                this.get('model').removeObjects(this.get('completed'));
                this.get('repo').persist();
            }
        }
    });
});
define('todomvc/controllers/completed', ['exports', 'ember'], function (exports, _ember) {
    exports['default'] = _ember['default'].Controller.extend({
        todos: _ember['default'].computed.filterBy('model', 'completed', true)
    });
});
define('todomvc/helpers/gt', ['exports', 'ember'], function (exports, _ember) {
    var _slicedToArray = (function () { function sliceIterator(arr, i) { var _arr = []; var _n = true; var _d = false; var _e = undefined; try { for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) { _arr.push(_s.value); if (i && _arr.length === i) break; } } catch (err) { _d = true; _e = err; } finally { try { if (!_n && _i['return']) _i['return'](); } finally { if (_d) throw _e; } } return _arr; } return function (arr, i) { if (Array.isArray(arr)) { return arr; } else if (Symbol.iterator in Object(arr)) { return sliceIterator(arr, i); } else { throw new TypeError('Invalid attempt to destructure non-iterable instance'); } }; })();

    exports.gt = gt;

    function gt(_ref /*, hash*/) {
        var _ref2 = _slicedToArray(_ref, 2);

        var n1 = _ref2[0];
        var n2 = _ref2[1];

        return n1 > n2;
    }

    exports['default'] = _ember['default'].Helper.helper(gt);
});
define('todomvc/helpers/pluralize', ['exports', 'ember', 'ember-inflector'], function (exports, _ember, _emberInflector) {
    var _slicedToArray = (function () { function sliceIterator(arr, i) { var _arr = []; var _n = true; var _d = false; var _e = undefined; try { for (var _i = arr[Symbol.iterator](), _s; !(_n = (_s = _i.next()).done); _n = true) { _arr.push(_s.value); if (i && _arr.length === i) break; } } catch (err) { _d = true; _e = err; } finally { try { if (!_n && _i['return']) _i['return'](); } finally { if (_d) throw _e; } } return _arr; } return function (arr, i) { if (Array.isArray(arr)) { return arr; } else if (Symbol.iterator in Object(arr)) { return sliceIterator(arr, i); } else { throw new TypeError('Invalid attempt to destructure non-iterable instance'); } }; })();

    exports.pluralizeHelper = pluralizeHelper;

    function pluralizeHelper(_ref /*, hash*/) {
        var _ref2 = _slicedToArray(_ref, 2);

        var singular = _ref2[0];
        var count = _ref2[1];

        return count === 1 ? singular : (0, _emberInflector.pluralize)(singular);
    }

    exports['default'] = _ember['default'].Helper.helper(pluralizeHelper);
});
define('todomvc/helpers/singularize', ['exports', 'ember-inflector/lib/helpers/singularize'], function (exports, _emberInflectorLibHelpersSingularize) {
  exports['default'] = _emberInflectorLibHelpersSingularize['default'];
});
define('todomvc/initializers/app-version', ['exports', 'ember-cli-app-version/initializer-factory', 'todomvc/config/environment'], function (exports, _emberCliAppVersionInitializerFactory, _todomvcConfigEnvironment) {
  exports['default'] = {
    name: 'App Version',
    initialize: (0, _emberCliAppVersionInitializerFactory['default'])(_todomvcConfigEnvironment['default'].APP.name, _todomvcConfigEnvironment['default'].APP.version)
  };
});
define('todomvc/initializers/container-debug-adapter', ['exports', 'ember-resolver/container-debug-adapter'], function (exports, _emberResolverContainerDebugAdapter) {
  exports['default'] = {
    name: 'container-debug-adapter',

    initialize: function initialize() {
      var app = arguments[1] || arguments[0];

      app.register('container-debug-adapter:main', _emberResolverContainerDebugAdapter['default']);
      app.inject('container-debug-adapter:main', 'namespace', 'application:main');
    }
  };
});
define('todomvc/initializers/export-application-global', ['exports', 'ember', 'todomvc/config/environment'], function (exports, _ember, _todomvcConfigEnvironment) {
  exports.initialize = initialize;

  function initialize() {
    var application = arguments[1] || arguments[0];
    if (_todomvcConfigEnvironment['default'].exportApplicationGlobal !== false) {
      var theGlobal;
      if (typeof window !== 'undefined') {
        theGlobal = window;
      } else if (typeof global !== 'undefined') {
        theGlobal = global;
      } else if (typeof self !== 'undefined') {
        theGlobal = self;
      } else {
        // no reasonable global, just bail
        return;
      }

      var value = _todomvcConfigEnvironment['default'].exportApplicationGlobal;
      var globalName;

      if (typeof value === 'string') {
        globalName = value;
      } else {
        globalName = _ember['default'].String.classify(_todomvcConfigEnvironment['default'].modulePrefix);
      }

      if (!theGlobal[globalName]) {
        theGlobal[globalName] = application;

        application.reopen({
          willDestroy: function willDestroy() {
            this._super.apply(this, arguments);
            delete theGlobal[globalName];
          }
        });
      }
    }
  }

  exports['default'] = {
    name: 'export-application-global',

    initialize: initialize
  };
});
define('todomvc/instance-initializers/global', ['exports'], function (exports) {
  exports.initialize = initialize;
  // app/instance-initializers/global.js

  function initialize(application) {
    window.App = application; // or window.Whatever
  }

  exports['default'] = {
    name: 'global',
    initialize: initialize
  };
});
define('todomvc/resolver', ['exports', 'ember-resolver'], function (exports, _emberResolver) {
  exports['default'] = _emberResolver['default'];
});
define('todomvc/router', ['exports', 'ember', 'todomvc/config/environment'], function (exports, _ember, _todomvcConfigEnvironment) {

    var Router = _ember['default'].Router.extend({
        location: _todomvcConfigEnvironment['default'].locationType
    });

    Router.map(function () {
        this.route('active');
        this.route('completed');
    });

    exports['default'] = Router;
});
define('todomvc/routes/application', ['exports', 'ember'], function (exports, _ember) {
    exports['default'] = _ember['default'].Route.extend({
        repo: _ember['default'].inject.service(),
        model: function model() {
            return this.get('repo').findAll();
        }
    });
});
define('todomvc/services/ajax', ['exports', 'ember-ajax/services/ajax'], function (exports, _emberAjaxServicesAjax) {
  Object.defineProperty(exports, 'default', {
    enumerable: true,
    get: function get() {
      return _emberAjaxServicesAjax['default'];
    }
  });
});
define('todomvc/services/memory', ['exports'], function (exports) {
  (function (root) {
    var localStorageMemory = {};
    var cache = {};

    /**
     * number of stored items.
     */
    localStorageMemory.length = 0;

    /**
     * returns item for passed key, or null
     *
     * @para {String} key
     *       name of item to be returned
     * @returns {String|null}
     */
    localStorageMemory.getItem = function (key) {
      return cache[key] || null;
    };

    /**
     * sets item for key to passed value, as String
     *
     * @para {String} key
     *       name of item to be set
     * @para {String} value
     *       value, will always be turned into a String
     * @returns {undefined}
     */
    localStorageMemory.setItem = function (key, value) {
      if (typeof value === 'undefined') {
        localStorageMemory.removeItem(key);
      } else {
        if (!cache.hasOwnProperty(key)) {
          localStorageMemory.length++;
        }

        cache[key] = '' + value;
      }
    };

    /**
     * removes item for passed key
     *
     * @para {String} key
     *       name of item to be removed
     * @returns {undefined}
     */
    localStorageMemory.removeItem = function (key) {
      if (cache.hasOwnProperty(key)) {
        delete cache[key];
        localStorageMemory.length--;
      }
    };

    /**
     * returns name of key at passed index
     *
     * @para {Number} index
     *       Position for key to be returned (starts at 0)
     * @returns {String|null}
     */
    localStorageMemory.key = function (index) {
      return Object.keys(cache)[index] || null;
    };

    /**
     * removes all stored items and sets length to 0
     *
     * @returns {undefined}
     */
    localStorageMemory.clear = function () {
      cache = {};
      localStorageMemory.length = 0;
    };

    root.localStorageMemory = localStorageMemory;
  })(window);
});
define('todomvc/services/repo', ['exports', 'ember', 'todomvc/services/memory'], function (exports, _ember, _todomvcServicesMemory) {
    exports['default'] = _ember['default'].Service.extend({
        lastId: 0,
        data: null,
        findAll: function findAll() {
            return this.get('data') || this.set('data', JSON.parse(window.localStorageMemory.getItem('todos') || '[]'));
        },

        add: function add(attrs) {
            var todo = Object.assign({ id: this.incrementProperty('lastId') }, attrs);
            this.get('data').pushObject(todo);
            this.persist();
            return todo;
        },

        'delete': function _delete(todo) {
            this.get('data').removeObject(todo);
            this.persist();
        },

        persist: function persist() {
            window.localStorageMemory.setItem('todos', JSON.stringify(this.get('data')));
        }
    });
});
define("todomvc/templates/active", ["exports"], function (exports) {
  exports["default"] = Ember.HTMLBars.template((function () {
    return {
      meta: {
        "fragmentReason": {
          "name": "missing-wrapper",
          "problems": ["wrong-type"]
        },
        "revision": "Ember@2.6.2",
        "loc": {
          "source": null,
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 1,
            "column": 25
          }
        },
        "moduleName": "todomvc/templates/active.hbs"
      },
      isEmpty: false,
      arity: 0,
      cachedFragment: null,
      hasRendered: false,
      buildFragment: function buildFragment(dom) {
        var el0 = dom.createDocumentFragment();
        var el1 = dom.createComment("");
        dom.appendChild(el0, el1);
        return el0;
      },
      buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
        var morphs = new Array(1);
        morphs[0] = dom.createMorphAt(fragment, 0, 0, contextualElement);
        dom.insertBoundary(fragment, 0);
        dom.insertBoundary(fragment, null);
        return morphs;
      },
      statements: [["inline", "todo-list", [], ["todos", ["subexpr", "@mut", [["get", "todos", ["loc", [null, [1, 18], [1, 23]]]]], [], []]], ["loc", [null, [1, 0], [1, 25]]]]],
      locals: [],
      templates: []
    };
  })());
});
define("todomvc/templates/application", ["exports"], function (exports) {
  exports["default"] = Ember.HTMLBars.template((function () {
    var child0 = (function () {
      var child0 = (function () {
        return {
          meta: {
            "fragmentReason": false,
            "revision": "Ember@2.6.2",
            "loc": {
              "source": null,
              "start": {
                "line": 11,
                "column": 14
              },
              "end": {
                "line": 11,
                "column": 60
              }
            },
            "moduleName": "todomvc/templates/application.hbs"
          },
          isEmpty: false,
          arity: 0,
          cachedFragment: null,
          hasRendered: false,
          buildFragment: function buildFragment(dom) {
            var el0 = dom.createDocumentFragment();
            var el1 = dom.createTextNode("All");
            dom.appendChild(el0, el1);
            return el0;
          },
          buildRenderNodes: function buildRenderNodes() {
            return [];
          },
          statements: [],
          locals: [],
          templates: []
        };
      })();
      var child1 = (function () {
        return {
          meta: {
            "fragmentReason": false,
            "revision": "Ember@2.6.2",
            "loc": {
              "source": null,
              "start": {
                "line": 12,
                "column": 14
              },
              "end": {
                "line": 12,
                "column": 64
              }
            },
            "moduleName": "todomvc/templates/application.hbs"
          },
          isEmpty: false,
          arity: 0,
          cachedFragment: null,
          hasRendered: false,
          buildFragment: function buildFragment(dom) {
            var el0 = dom.createDocumentFragment();
            var el1 = dom.createTextNode("Active");
            dom.appendChild(el0, el1);
            return el0;
          },
          buildRenderNodes: function buildRenderNodes() {
            return [];
          },
          statements: [],
          locals: [],
          templates: []
        };
      })();
      var child2 = (function () {
        return {
          meta: {
            "fragmentReason": false,
            "revision": "Ember@2.6.2",
            "loc": {
              "source": null,
              "start": {
                "line": 13,
                "column": 14
              },
              "end": {
                "line": 13,
                "column": 70
              }
            },
            "moduleName": "todomvc/templates/application.hbs"
          },
          isEmpty: false,
          arity: 0,
          cachedFragment: null,
          hasRendered: false,
          buildFragment: function buildFragment(dom) {
            var el0 = dom.createDocumentFragment();
            var el1 = dom.createTextNode("Completed");
            dom.appendChild(el0, el1);
            return el0;
          },
          buildRenderNodes: function buildRenderNodes() {
            return [];
          },
          statements: [],
          locals: [],
          templates: []
        };
      })();
      var child3 = (function () {
        return {
          meta: {
            "fragmentReason": false,
            "revision": "Ember@2.6.2",
            "loc": {
              "source": null,
              "start": {
                "line": 15,
                "column": 8
              },
              "end": {
                "line": 17,
                "column": 8
              }
            },
            "moduleName": "todomvc/templates/application.hbs"
          },
          isEmpty: false,
          arity: 0,
          cachedFragment: null,
          hasRendered: false,
          buildFragment: function buildFragment(dom) {
            var el0 = dom.createDocumentFragment();
            var el1 = dom.createTextNode("          ");
            dom.appendChild(el0, el1);
            var el1 = dom.createElement("button");
            dom.setAttribute(el1, "id", "clear-completed");
            var el2 = dom.createTextNode("Clear completed");
            dom.appendChild(el1, el2);
            dom.appendChild(el0, el1);
            var el1 = dom.createTextNode("\n");
            dom.appendChild(el0, el1);
            return el0;
          },
          buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
            var element0 = dom.childAt(fragment, [1]);
            var morphs = new Array(1);
            morphs[0] = dom.createAttrMorph(element0, 'onclick');
            return morphs;
          },
          statements: [["attribute", "onclick", ["subexpr", "action", ["clearCompleted"], [], ["loc", [null, [16, 47], [16, 74]]]]]],
          locals: [],
          templates: []
        };
      })();
      return {
        meta: {
          "fragmentReason": false,
          "revision": "Ember@2.6.2",
          "loc": {
            "source": null,
            "start": {
              "line": 7,
              "column": 4
            },
            "end": {
              "line": 19,
              "column": 4
            }
          },
          "moduleName": "todomvc/templates/application.hbs"
        },
        isEmpty: false,
        arity: 0,
        cachedFragment: null,
        hasRendered: false,
        buildFragment: function buildFragment(dom) {
          var el0 = dom.createDocumentFragment();
          var el1 = dom.createTextNode("      ");
          dom.appendChild(el0, el1);
          var el1 = dom.createElement("footer");
          dom.setAttribute(el1, "id", "footer");
          var el2 = dom.createTextNode("\n        ");
          dom.appendChild(el1, el2);
          var el2 = dom.createElement("span");
          dom.setAttribute(el2, "id", "todo-count");
          var el3 = dom.createElement("strong");
          var el4 = dom.createComment("");
          dom.appendChild(el3, el4);
          dom.appendChild(el2, el3);
          var el3 = dom.createTextNode(" ");
          dom.appendChild(el2, el3);
          var el3 = dom.createComment("");
          dom.appendChild(el2, el3);
          var el3 = dom.createTextNode(" left");
          dom.appendChild(el2, el3);
          dom.appendChild(el1, el2);
          var el2 = dom.createTextNode("\n        ");
          dom.appendChild(el1, el2);
          var el2 = dom.createElement("ul");
          dom.setAttribute(el2, "id", "filters");
          var el3 = dom.createTextNode("\n          ");
          dom.appendChild(el2, el3);
          var el3 = dom.createElement("li");
          var el4 = dom.createComment("");
          dom.appendChild(el3, el4);
          dom.appendChild(el2, el3);
          var el3 = dom.createTextNode("\n          ");
          dom.appendChild(el2, el3);
          var el3 = dom.createElement("li");
          var el4 = dom.createComment("");
          dom.appendChild(el3, el4);
          dom.appendChild(el2, el3);
          var el3 = dom.createTextNode("\n          ");
          dom.appendChild(el2, el3);
          var el3 = dom.createElement("li");
          var el4 = dom.createComment("");
          dom.appendChild(el3, el4);
          dom.appendChild(el2, el3);
          var el3 = dom.createTextNode("\n        ");
          dom.appendChild(el2, el3);
          dom.appendChild(el1, el2);
          var el2 = dom.createTextNode("\n");
          dom.appendChild(el1, el2);
          var el2 = dom.createComment("");
          dom.appendChild(el1, el2);
          var el2 = dom.createTextNode("      ");
          dom.appendChild(el1, el2);
          dom.appendChild(el0, el1);
          var el1 = dom.createTextNode("\n");
          dom.appendChild(el0, el1);
          return el0;
        },
        buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
          var element1 = dom.childAt(fragment, [1]);
          var element2 = dom.childAt(element1, [1]);
          var element3 = dom.childAt(element1, [3]);
          var morphs = new Array(6);
          morphs[0] = dom.createMorphAt(dom.childAt(element2, [0]), 0, 0);
          morphs[1] = dom.createMorphAt(element2, 2, 2);
          morphs[2] = dom.createMorphAt(dom.childAt(element3, [1]), 0, 0);
          morphs[3] = dom.createMorphAt(dom.childAt(element3, [3]), 0, 0);
          morphs[4] = dom.createMorphAt(dom.childAt(element3, [5]), 0, 0);
          morphs[5] = dom.createMorphAt(element1, 5, 5);
          return morphs;
        },
        statements: [["content", "remaining.length", ["loc", [null, [9, 38], [9, 58]]]], ["inline", "pluralize", ["item", ["get", "remaining.length", ["loc", [null, [9, 87], [9, 103]]]]], [], ["loc", [null, [9, 68], [9, 105]]]], ["block", "link-to", ["index"], ["activeClass", "selected"], 0, null, ["loc", [null, [11, 14], [11, 72]]]], ["block", "link-to", ["active"], ["activeClass", "selected"], 1, null, ["loc", [null, [12, 14], [12, 76]]]], ["block", "link-to", ["completed"], ["activeClass", "selected"], 2, null, ["loc", [null, [13, 14], [13, 82]]]], ["block", "if", [["get", "completed.length", ["loc", [null, [15, 14], [15, 30]]]]], [], 3, null, ["loc", [null, [15, 8], [17, 15]]]]],
        locals: [],
        templates: [child0, child1, child2, child3]
      };
    })();
    return {
      meta: {
        "fragmentReason": {
          "name": "missing-wrapper",
          "problems": ["multiple-nodes"]
        },
        "revision": "Ember@2.6.2",
        "loc": {
          "source": null,
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 29,
            "column": 9
          }
        },
        "moduleName": "todomvc/templates/application.hbs"
      },
      isEmpty: false,
      arity: 0,
      cachedFragment: null,
      hasRendered: false,
      buildFragment: function buildFragment(dom) {
        var el0 = dom.createDocumentFragment();
        var el1 = dom.createElement("section");
        dom.setAttribute(el1, "id", "todoapp");
        var el2 = dom.createTextNode("\n  ");
        dom.appendChild(el1, el2);
        var el2 = dom.createElement("header");
        dom.setAttribute(el2, "id", "header");
        var el3 = dom.createTextNode("\n    ");
        dom.appendChild(el2, el3);
        var el3 = dom.createElement("h1");
        var el4 = dom.createTextNode("todos");
        dom.appendChild(el3, el4);
        dom.appendChild(el2, el3);
        var el3 = dom.createTextNode("\n    ");
        dom.appendChild(el2, el3);
        var el3 = dom.createElement("input");
        dom.setAttribute(el3, "type", "text");
        dom.setAttribute(el3, "id", "new-todo");
        dom.setAttribute(el3, "placeholder", "What needs to be done?");
        dom.setAttribute(el3, "autofocus", "");
        dom.appendChild(el2, el3);
        var el3 = dom.createTextNode("\n  ");
        dom.appendChild(el2, el3);
        dom.appendChild(el1, el2);
        var el2 = dom.createTextNode("\n    ");
        dom.appendChild(el1, el2);
        var el2 = dom.createComment("");
        dom.appendChild(el1, el2);
        var el2 = dom.createTextNode("\n");
        dom.appendChild(el1, el2);
        var el2 = dom.createComment("");
        dom.appendChild(el1, el2);
        dom.appendChild(el0, el1);
        var el1 = dom.createTextNode("\n");
        dom.appendChild(el0, el1);
        var el1 = dom.createElement("footer");
        dom.setAttribute(el1, "id", "info");
        var el2 = dom.createTextNode("\n  ");
        dom.appendChild(el1, el2);
        var el2 = dom.createElement("p");
        var el3 = dom.createTextNode("Double-click to edit a todo");
        dom.appendChild(el2, el3);
        dom.appendChild(el1, el2);
        var el2 = dom.createTextNode("\n  ");
        dom.appendChild(el1, el2);
        var el2 = dom.createElement("p");
        var el3 = dom.createTextNode("\n    Created by\n    ");
        dom.appendChild(el2, el3);
        var el3 = dom.createElement("a");
        dom.setAttribute(el3, "href", "http://github.com/cibernox");
        var el4 = dom.createTextNode("Miguel Camba");
        dom.appendChild(el3, el4);
        dom.appendChild(el2, el3);
        var el3 = dom.createTextNode(",\n    ");
        dom.appendChild(el2, el3);
        var el3 = dom.createElement("a");
        dom.setAttribute(el3, "href", "http://github.com/addyosmani");
        var el4 = dom.createTextNode("Addy Osmani");
        dom.appendChild(el3, el4);
        dom.appendChild(el2, el3);
        var el3 = dom.createTextNode("\n  ");
        dom.appendChild(el2, el3);
        dom.appendChild(el1, el2);
        var el2 = dom.createTextNode("\n  ");
        dom.appendChild(el1, el2);
        var el2 = dom.createElement("p");
        var el3 = dom.createTextNode("Part of ");
        dom.appendChild(el2, el3);
        var el3 = dom.createElement("a");
        dom.setAttribute(el3, "href", "http://todomvc.com");
        var el4 = dom.createTextNode("TodoMVC");
        dom.appendChild(el3, el4);
        dom.appendChild(el2, el3);
        dom.appendChild(el1, el2);
        var el2 = dom.createTextNode("\n");
        dom.appendChild(el1, el2);
        dom.appendChild(el0, el1);
        return el0;
      },
      buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
        var element4 = dom.childAt(fragment, [0]);
        var element5 = dom.childAt(element4, [1, 3]);
        var morphs = new Array(3);
        morphs[0] = dom.createAttrMorph(element5, 'onkeydown');
        morphs[1] = dom.createMorphAt(element4, 3, 3);
        morphs[2] = dom.createMorphAt(element4, 5, 5);
        return morphs;
      },
      statements: [["attribute", "onkeydown", ["subexpr", "action", ["createTodo"], [], ["loc", [null, [4, 47], [4, 70]]]]], ["content", "outlet", ["loc", [null, [6, 4], [6, 14]]]], ["block", "if", [["subexpr", "gt", [["get", "model.length", ["loc", [null, [7, 14], [7, 26]]]], 0], [], ["loc", [null, [7, 10], [7, 29]]]]], [], 0, null, ["loc", [null, [7, 4], [19, 11]]]]],
      locals: [],
      templates: [child0]
    };
  })());
});
define("todomvc/templates/completed", ["exports"], function (exports) {
  exports["default"] = Ember.HTMLBars.template((function () {
    return {
      meta: {
        "fragmentReason": {
          "name": "missing-wrapper",
          "problems": ["wrong-type"]
        },
        "revision": "Ember@2.6.2",
        "loc": {
          "source": null,
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 1,
            "column": 25
          }
        },
        "moduleName": "todomvc/templates/completed.hbs"
      },
      isEmpty: false,
      arity: 0,
      cachedFragment: null,
      hasRendered: false,
      buildFragment: function buildFragment(dom) {
        var el0 = dom.createDocumentFragment();
        var el1 = dom.createComment("");
        dom.appendChild(el0, el1);
        return el0;
      },
      buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
        var morphs = new Array(1);
        morphs[0] = dom.createMorphAt(fragment, 0, 0, contextualElement);
        dom.insertBoundary(fragment, 0);
        dom.insertBoundary(fragment, null);
        return morphs;
      },
      statements: [["inline", "todo-list", [], ["todos", ["subexpr", "@mut", [["get", "todos", ["loc", [null, [1, 18], [1, 23]]]]], [], []]], ["loc", [null, [1, 0], [1, 25]]]]],
      locals: [],
      templates: []
    };
  })());
});
define("todomvc/templates/components/todo-item", ["exports"], function (exports) {
  exports["default"] = Ember.HTMLBars.template((function () {
    return {
      meta: {
        "fragmentReason": {
          "name": "missing-wrapper",
          "problems": ["multiple-nodes"]
        },
        "revision": "Ember@2.6.2",
        "loc": {
          "source": null,
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 6,
            "column": 153
          }
        },
        "moduleName": "todomvc/templates/components/todo-item.hbs"
      },
      isEmpty: false,
      arity: 0,
      cachedFragment: null,
      hasRendered: false,
      buildFragment: function buildFragment(dom) {
        var el0 = dom.createDocumentFragment();
        var el1 = dom.createElement("div");
        dom.setAttribute(el1, "class", "view");
        var el2 = dom.createTextNode("\n  ");
        dom.appendChild(el1, el2);
        var el2 = dom.createElement("input");
        dom.setAttribute(el2, "type", "checkbox");
        dom.setAttribute(el2, "class", "toggle");
        dom.appendChild(el1, el2);
        var el2 = dom.createTextNode("\n  ");
        dom.appendChild(el1, el2);
        var el2 = dom.createElement("label");
        var el3 = dom.createComment("");
        dom.appendChild(el2, el3);
        dom.appendChild(el1, el2);
        var el2 = dom.createTextNode("\n  ");
        dom.appendChild(el1, el2);
        var el2 = dom.createElement("button");
        dom.setAttribute(el2, "class", "destroy");
        dom.appendChild(el1, el2);
        var el2 = dom.createTextNode("\n");
        dom.appendChild(el1, el2);
        dom.appendChild(el0, el1);
        var el1 = dom.createTextNode("\n");
        dom.appendChild(el0, el1);
        var el1 = dom.createElement("input");
        dom.setAttribute(el1, "type", "text");
        dom.setAttribute(el1, "class", "edit");
        dom.setAttribute(el1, "autofocus", "");
        dom.appendChild(el0, el1);
        return el0;
      },
      buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
        var element0 = dom.childAt(fragment, [0]);
        var element1 = dom.childAt(element0, [1]);
        if (this.cachedFragment) {
          dom.repairClonedNode(element1, [], true);
        }
        var element2 = dom.childAt(element0, [3]);
        var element3 = dom.childAt(element0, [5]);
        var element4 = dom.childAt(fragment, [2]);
        var morphs = new Array(8);
        morphs[0] = dom.createAttrMorph(element1, 'checked');
        morphs[1] = dom.createAttrMorph(element1, 'onchange');
        morphs[2] = dom.createAttrMorph(element2, 'ondblclick');
        morphs[3] = dom.createMorphAt(element2, 0, 0);
        morphs[4] = dom.createAttrMorph(element3, 'onclick');
        morphs[5] = dom.createAttrMorph(element4, 'value');
        morphs[6] = dom.createAttrMorph(element4, 'onblur');
        morphs[7] = dom.createAttrMorph(element4, 'onkeydown');
        return morphs;
      },
      statements: [["attribute", "checked", ["get", "todo.completed", ["loc", [null, [2, 50], [2, 64]]]]], ["attribute", "onchange", ["subexpr", "action", ["toggleCompleted"], [], ["loc", [null, [2, 76], [2, 104]]]]], ["attribute", "ondblclick", ["subexpr", "action", ["startEditing"], [], ["loc", [null, [3, 20], [3, 45]]]]], ["content", "todo.title", ["loc", [null, [3, 46], [3, 60]]]], ["attribute", "onclick", ["subexpr", "action", ["removeTodo"], [], ["loc", [null, [4, 18], [4, 41]]]]], ["attribute", "value", ["get", "todo.title", ["loc", [null, [6, 40], [6, 50]]]]], ["attribute", "onblur", ["subexpr", "action", ["doneEditing"], ["value", "target.value"], ["loc", [null, [6, 60], [6, 105]]]]], ["attribute", "onkeydown", ["subexpr", "action", ["handleKeydown"], [], ["loc", [null, [6, 116], [6, 142]]]]]],
      locals: [],
      templates: []
    };
  })());
});
define("todomvc/templates/components/todo-list", ["exports"], function (exports) {
  exports["default"] = Ember.HTMLBars.template((function () {
    var child0 = (function () {
      var child0 = (function () {
        return {
          meta: {
            "fragmentReason": false,
            "revision": "Ember@2.6.2",
            "loc": {
              "source": null,
              "start": {
                "line": 2,
                "column": 2
              },
              "end": {
                "line": 4,
                "column": 2
              }
            },
            "moduleName": "todomvc/templates/components/todo-list.hbs"
          },
          isEmpty: false,
          arity: 0,
          cachedFragment: null,
          hasRendered: false,
          buildFragment: function buildFragment(dom) {
            var el0 = dom.createDocumentFragment();
            var el1 = dom.createTextNode("    ");
            dom.appendChild(el0, el1);
            var el1 = dom.createElement("input");
            dom.setAttribute(el1, "type", "checkbox");
            dom.setAttribute(el1, "id", "toggle-all");
            dom.appendChild(el0, el1);
            var el1 = dom.createTextNode("\n");
            dom.appendChild(el0, el1);
            return el0;
          },
          buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
            var element0 = dom.childAt(fragment, [1]);
            if (this.cachedFragment) {
              dom.repairClonedNode(element0, [], true);
            }
            var morphs = new Array(2);
            morphs[0] = dom.createAttrMorph(element0, 'checked');
            morphs[1] = dom.createAttrMorph(element0, 'onchange');
            return morphs;
          },
          statements: [["attribute", "checked", ["get", "allCompleted", ["loc", [null, [3, 53], [3, 65]]]]], ["attribute", "onchange", ["subexpr", "action", ["toggleAll"], [], ["loc", [null, [3, 77], [3, 99]]]]]],
          locals: [],
          templates: []
        };
      })();
      var child1 = (function () {
        return {
          meta: {
            "fragmentReason": false,
            "revision": "Ember@2.6.2",
            "loc": {
              "source": null,
              "start": {
                "line": 6,
                "column": 4
              },
              "end": {
                "line": 8,
                "column": 4
              }
            },
            "moduleName": "todomvc/templates/components/todo-list.hbs"
          },
          isEmpty: false,
          arity: 1,
          cachedFragment: null,
          hasRendered: false,
          buildFragment: function buildFragment(dom) {
            var el0 = dom.createDocumentFragment();
            var el1 = dom.createTextNode("      ");
            dom.appendChild(el0, el1);
            var el1 = dom.createComment("");
            dom.appendChild(el0, el1);
            var el1 = dom.createTextNode("\n");
            dom.appendChild(el0, el1);
            return el0;
          },
          buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
            var morphs = new Array(1);
            morphs[0] = dom.createMorphAt(fragment, 1, 1, contextualElement);
            return morphs;
          },
          statements: [["inline", "todo-item", [], ["todo", ["subexpr", "@mut", [["get", "todo", ["loc", [null, [7, 23], [7, 27]]]]], [], []], "onStartEdit", ["subexpr", "action", ["disableToggle"], [], ["loc", [null, [7, 40], [7, 64]]]], "onEndEdit", ["subexpr", "action", ["enableToggle"], [], ["loc", [null, [7, 75], [7, 98]]]]], ["loc", [null, [7, 6], [7, 100]]]]],
          locals: ["todo"],
          templates: []
        };
      })();
      return {
        meta: {
          "fragmentReason": {
            "name": "missing-wrapper",
            "problems": ["wrong-type", "multiple-nodes"]
          },
          "revision": "Ember@2.6.2",
          "loc": {
            "source": null,
            "start": {
              "line": 1,
              "column": 0
            },
            "end": {
              "line": 10,
              "column": 0
            }
          },
          "moduleName": "todomvc/templates/components/todo-list.hbs"
        },
        isEmpty: false,
        arity: 0,
        cachedFragment: null,
        hasRendered: false,
        buildFragment: function buildFragment(dom) {
          var el0 = dom.createDocumentFragment();
          var el1 = dom.createComment("");
          dom.appendChild(el0, el1);
          var el1 = dom.createTextNode("  ");
          dom.appendChild(el0, el1);
          var el1 = dom.createElement("ul");
          dom.setAttribute(el1, "id", "todo-list");
          dom.setAttribute(el1, "class", "todo-list");
          var el2 = dom.createTextNode("\n");
          dom.appendChild(el1, el2);
          var el2 = dom.createComment("");
          dom.appendChild(el1, el2);
          var el2 = dom.createTextNode("  ");
          dom.appendChild(el1, el2);
          dom.appendChild(el0, el1);
          var el1 = dom.createTextNode("\n");
          dom.appendChild(el0, el1);
          return el0;
        },
        buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
          var morphs = new Array(2);
          morphs[0] = dom.createMorphAt(fragment, 0, 0, contextualElement);
          morphs[1] = dom.createMorphAt(dom.childAt(fragment, [2]), 1, 1);
          dom.insertBoundary(fragment, 0);
          return morphs;
        },
        statements: [["block", "if", [["get", "canToggle", ["loc", [null, [2, 8], [2, 17]]]]], [], 0, null, ["loc", [null, [2, 2], [4, 9]]]], ["block", "each", [["get", "todos", ["loc", [null, [6, 12], [6, 17]]]]], [], 1, null, ["loc", [null, [6, 4], [8, 13]]]]],
        locals: [],
        templates: [child0, child1]
      };
    })();
    return {
      meta: {
        "fragmentReason": {
          "name": "missing-wrapper",
          "problems": ["wrong-type"]
        },
        "revision": "Ember@2.6.2",
        "loc": {
          "source": null,
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 11,
            "column": 0
          }
        },
        "moduleName": "todomvc/templates/components/todo-list.hbs"
      },
      isEmpty: false,
      arity: 0,
      cachedFragment: null,
      hasRendered: false,
      buildFragment: function buildFragment(dom) {
        var el0 = dom.createDocumentFragment();
        var el1 = dom.createComment("");
        dom.appendChild(el0, el1);
        return el0;
      },
      buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
        var morphs = new Array(1);
        morphs[0] = dom.createMorphAt(fragment, 0, 0, contextualElement);
        dom.insertBoundary(fragment, 0);
        dom.insertBoundary(fragment, null);
        return morphs;
      },
      statements: [["block", "if", [["get", "todos.length", ["loc", [null, [1, 6], [1, 18]]]]], [], 0, null, ["loc", [null, [1, 0], [10, 7]]]]],
      locals: [],
      templates: [child0]
    };
  })());
});
define("todomvc/templates/index", ["exports"], function (exports) {
  exports["default"] = Ember.HTMLBars.template((function () {
    var child0 = (function () {
      return {
        meta: {
          "fragmentReason": {
            "name": "missing-wrapper",
            "problems": ["wrong-type"]
          },
          "revision": "Ember@2.6.2",
          "loc": {
            "source": null,
            "start": {
              "line": 1,
              "column": 0
            },
            "end": {
              "line": 3,
              "column": 0
            }
          },
          "moduleName": "todomvc/templates/index.hbs"
        },
        isEmpty: false,
        arity: 0,
        cachedFragment: null,
        hasRendered: false,
        buildFragment: function buildFragment(dom) {
          var el0 = dom.createDocumentFragment();
          var el1 = dom.createTextNode("  ");
          dom.appendChild(el0, el1);
          var el1 = dom.createComment("");
          dom.appendChild(el0, el1);
          var el1 = dom.createTextNode("\n");
          dom.appendChild(el0, el1);
          return el0;
        },
        buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
          var morphs = new Array(1);
          morphs[0] = dom.createMorphAt(fragment, 1, 1, contextualElement);
          return morphs;
        },
        statements: [["inline", "todo-list", [], ["todos", ["subexpr", "@mut", [["get", "model", ["loc", [null, [2, 20], [2, 25]]]]], [], []]], ["loc", [null, [2, 2], [2, 27]]]]],
        locals: [],
        templates: []
      };
    })();
    return {
      meta: {
        "fragmentReason": {
          "name": "missing-wrapper",
          "problems": ["wrong-type"]
        },
        "revision": "Ember@2.6.2",
        "loc": {
          "source": null,
          "start": {
            "line": 1,
            "column": 0
          },
          "end": {
            "line": 4,
            "column": 0
          }
        },
        "moduleName": "todomvc/templates/index.hbs"
      },
      isEmpty: false,
      arity: 0,
      cachedFragment: null,
      hasRendered: false,
      buildFragment: function buildFragment(dom) {
        var el0 = dom.createDocumentFragment();
        var el1 = dom.createComment("");
        dom.appendChild(el0, el1);
        return el0;
      },
      buildRenderNodes: function buildRenderNodes(dom, fragment, contextualElement) {
        var morphs = new Array(1);
        morphs[0] = dom.createMorphAt(fragment, 0, 0, contextualElement);
        dom.insertBoundary(fragment, 0);
        dom.insertBoundary(fragment, null);
        return morphs;
      },
      statements: [["block", "if", [["get", "model.length", ["loc", [null, [1, 6], [1, 18]]]]], [], 0, null, ["loc", [null, [1, 0], [3, 7]]]]],
      locals: [],
      templates: [child0]
    };
  })());
});
/* jshint ignore:start */



/* jshint ignore:end */

/* jshint ignore:start */

define('todomvc/config/environment', ['ember'], function(Ember) {
  var prefix = 'todomvc';
/* jshint ignore:start */

try {
  var metaName = prefix + '/config/environment';
  var rawConfig = Ember['default'].$('meta[name="' + metaName + '"]').attr('content');
  var config = JSON.parse(unescape(rawConfig));

  return { 'default': config };
}
catch(err) {
  throw new Error('Could not read config from meta tag with name "' + metaName + '".');
}

/* jshint ignore:end */

});

/* jshint ignore:end */

/* jshint ignore:start */

if (!runningTests) {
  require("todomvc/app")["default"].create({"name":"todomvc","version":"0.0.0+"});
}

/* jshint ignore:end */
//# sourceMappingURL=todomvc.map
