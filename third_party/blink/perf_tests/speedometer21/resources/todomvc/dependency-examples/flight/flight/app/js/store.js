/*global define */

'use strict';

define([
    'depot',
    'memorystorage'
], function (depot, MemoryStorage) {
    var memStore = new MemoryStorage('todos');
    return depot('todos', { idAttribute: 'id', storageAdaptor: memStore });
});
