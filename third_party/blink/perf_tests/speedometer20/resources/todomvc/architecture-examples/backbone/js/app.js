/*global $ */
/*jshint unused:false */
var app = app || {};
var ENTER_KEY = 13;
var ESC_KEY = 27;

$(function () {
    'use strict';

    // kick things off by creating the `App`
    window.appView = new app.AppView();

    var dummyNodeToNotifyAppIsReady = document.createElement('div');
    dummyNodeToNotifyAppIsReady.id = 'appIsReady';
    document.body.appendChild(dummyNodeToNotifyAppIsReady);
});
