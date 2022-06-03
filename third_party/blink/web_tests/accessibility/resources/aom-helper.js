function enableAccessibilityEventsPermission() {
  return new Promise(function(resolve, reject) {
    PermissionsHelper.setPermission(
        'accessibility-events', 'granted').then(function() {
      // Don't resolve the promise until the permission has propagated
      // to AXObjectCacheImpl.
      let timerId;
      function checkPermission() {
        if (accessibilityController.canCallAOMEventListeners) {
          window.clearInterval(timerId);
          resolve();
        }
      }
      timerId = window.setInterval(checkPermission, 0);
    });
  });
}
