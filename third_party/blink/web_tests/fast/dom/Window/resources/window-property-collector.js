function collectProperties(object, windowHasBeenGCed)
{
    collectPropertiesHelper(object, object, windowHasBeenGCed, []);

    propertiesToVerify.sort(function (a, b)
    {
        if (a.property < b.property)
            return -1
        if (a.property > b.property)
            return 1;
        return 0;
    });
}

function emitExpectedResult(path, expected)
{
    // Skip internals properties, since they aren't web accessible.
    if (path[0] == 'internals'
        || path[0] == 'clientInformation' // Just an alias for navigator.
        || path[0] == 'testRunner' // Skip testRunner since they are only for testing.
        || path[0] == 'eventSender'// Skip eventSender since they are only for testing.
        || path[1] == 'gpuBenchmarking') { // Skip gpuBenchmarking since they're only for testing.
        return;
    }

    // Skip the properties which are hard to expect a stable result.
    if (path[0] == 'accessibilityController' // we can hardly estimate the states of the cached WebAXObjects.
        // TODO(https://crbug.com/698610): Web storage APIs are not being
        // cleared between tests.
        || path[0] == 'localStorage'
        || path[0] == 'sessionStorage') {
        return;
    }

    // Skip history, which throws SecurityErrors and is covered by web-platform-tests.
    if (path[0] == 'history')
        return;

    // FIXME: Skip MemoryInfo for now, since it's not implemented as a DOMWindowProperty, and has
    // no way of knowing when it's detached. Eventually this should have the same behavior.
    if (path.length >= 2 && (path[0] == 'console' || path[0] == 'performance') && path[1] == 'memory')
        return;

    // Skip things that are assumed to be constants.
    if (path[path.length - 1].toUpperCase() == path[path.length - 1])
        return;

    // Various special cases for legacy reasons. Please do not add entries to this list.
    var propertyPath = path.join('.');

    // Connection type depends on the host, skip.
    if (propertyPath == 'navigator.connection.type')
      return;
    if (propertyPath == 'navigator.connection.downlinkMax')
      return;
    if (propertyPath == 'navigator.connection.effectiveType')
      return;
    if (propertyPath == 'navigator.connection.rtt')
      return;
    if (propertyPath == 'navigator.connection.downlink')
      return;
    // timeOrigin is variable, skip.
    if (propertyPath == 'performance.timeOrigin')
      return;

    switch (propertyPath) {
    case "location.href":
        expected = "'about:blank'";
        break;
    case "location.origin":
        expected = "'null'";
        break;
    case "origin":
        expected = "'file://'";
        break;
    case "location.pathname":
        expected = "'blank'";
        break;
    case "location.protocol":
        expected = "'about:'";
        break;
    case "location.ancestorOrigins.length":
        expected = "1";
        break;
    case "navigator.appCodeName":
    case "navigator.appName":
    case "navigator.deviceMemory":
    case "navigator.hardwareConcurrency":
    case "navigator.language":
    case "navigator.onLine":
    case "navigator.platform":
    case "navigator.product":
    case "navigator.productSub":
    case "navigator.vendor":
        expected = "window." + propertyPath;
        break;
    case "navigator.mediaSession.playbackState":
        expected = "'none'";
        break;
    case "screen.orientation.type":
        expected = "'portrait-primary'";
        break;
    case "history.scrollRestoration":
        expected = "'auto'";
        break;
    case "scheduler.currentTaskQueue.priority":
        expected = "'default'";
        break;
    }

    insertExpectedResult(path, expected);
}

function collectPropertiesHelper(global, object, windowHasBeenGCed, path)
{
    if (path.length > 20)
        throw 'Error: probably looping';

    for (var property in object) {
        // Skip internals properties, since they aren't web accessible.
        if (property === 'internals')
            continue;
        path.push(property);
        var type = typeof(object[property]);
        if (type == "object") {
            if (object[property] === null) {
                emitExpectedResult(path, "null");
            } else if (!object[property].Window
                && !(object[property] instanceof global.Node)
                && !(object[property] instanceof global.MimeTypeArray)
                && !(object[property] instanceof global.PluginArray)
                && !(object[property] instanceof HTMLElement)) {
                // Skip some traversing through types that will end up in cycles...
                collectPropertiesHelper(global, object[property], windowHasBeenGCed, path);
            }
        } else if (type == "string") {
            emitExpectedResult(path, "''");
        } else if (type == "number") {
            emitExpectedResult(path, "0");
        } else if (type == "boolean") {
            expected = "false";
            if (path == "closed" && windowHasBeenGCed )
                expected = "true";
            emitExpectedResult(path, expected);
        }
        path.pop();
    }
}

function pathExists(object, path) {
    for (var i = 0; i < path.length; i++) {
        if (!object || !(path[i] in object))
            return false;
        object = object[path[i]];
    }
    return true;
}
