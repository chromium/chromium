/*global angular */

/**
 * Services that persists and retrieves todos from memory or a backend API
 * if available.
 *
 * They both follow the same API, returning promises for all changes to the
 * model.
 */
angular.module('todomvc')
    .factory('todoStorage', function ($http, $injector) {
        'use strict';
        return Promise.resolve().then(function() {
            return $injector.get('localCache');
        });
    })

    .factory('localCache', function ($q) {
        'use strict';

        var STORAGE_ID = 'todos-angularjs';

        var store = {
            todos: [],
            cache: [],

            _getFromLocalCache: function () {
                // return JSON.parse(this.cache[STORAGE_ID] || '[]');
                return [];
            },

            _saveToLocalCache: function (todos) {
                // this.cache[STORAGE_ID] = JSON.stringify(todos);
                return;
            },

            clearCompleted: function () {
                var deferred = $q.defer();

                var incompleteTodos = store.todos.filter(function (todo) {
                    return !todo.completed;
                });

                angular.copy(incompleteTodos, store.todos);

                store._saveToLocalCache(store.todos);
                deferred.resolve(store.todos);

                return deferred.promise;
            },

            delete: function (todo) {
                var deferred = $q.defer();

                store.todos.splice(store.todos.indexOf(todo), 1);

                store._saveToLocalCache(store.todos);
                deferred.resolve(store.todos);

                return deferred.promise;
            },

            get: function () {
                var deferred = $q.defer();

                angular.copy(store._getFromLocalCache(), store.todos);
                deferred.resolve(store.todos);

                return deferred.promise;
            },

            insert: function (todo) {
                var deferred = $q.defer();

                store.todos.push(todo);

                store._saveToLocalCache(store.todos);
                deferred.resolve(store.todos);

                return deferred.promise;
            },

            put: function (todo, index) {
                var deferred = $q.defer();

                store.todos[index] = todo;

                store._saveToLocalCache(store.todos);
                deferred.resolve(store.todos);

                return deferred.promise;
            }
        };

        return store;
    });
