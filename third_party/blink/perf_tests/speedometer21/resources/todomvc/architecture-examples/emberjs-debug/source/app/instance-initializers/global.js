// app/instance-initializers/global.js

export function initialize(application) {
  window.App = application;  // or window.Whatever
}

export default {
  name: 'global',
  initialize: initialize
};