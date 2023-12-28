function collectProperties(object)
{
    collectPropertiesHelper(object, object, []);

    propertiesToVerify.sort(function (a, b)
    {
        if (a.property < b.property)
            return -1
        if (a.property > b.property)
            return 1;
        return 0;
    });
}

function getPropertyPath(path, length)
{
    var propertyDir = path[0];
    for (var i = 1; i < length; ++i) {
      var part = path[i];
      if (part == "0") {
        propertyDir += "[" + part + "]";
      } else {
        propertyDir += "." + part;
      }
    }
    return propertyDir;
}

function emitExpectedResult(path, expected)
{
    if (path[0] == 'clientInformation' // Just an alias for navigator.
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

    // Special cases where the properties might return something other than the
    // "expected" default (e.g. bool property defaulting to false). Please do
    // not add exceptions to this list without documenting them.
    var propertyPath = getPropertyPath(path, path.length);

    // Properties that are skipped because they are unstable due to dependency
    // on system global state that is variable between test runs.
    switch (propertyPath) {
    // navigator.connection.downlink is an estimate based on recently observed
    // application layer throughput across recently active connections.
    case "navigator.connection.downlink":
    // performance.timeOrigin depends on when the page is loaded and is variable.
    case "performance.timeOrigin":
    // It's expected that performance.eventCounts.size is non-zero.
    case "performance.eventCounts.size":
        return;
    }

    switch (propertyPath) {
    // Various navigator properties that depend on the host. Check that they
    // match the property values of the top-level window.
    case "navigator.appCodeName":
    case "navigator.appName":
    case 'navigator.connection.type':
    case 'navigator.connection.downlinkMax':
    case 'navigator.connection.effectiveType':
    case 'navigator.connection.rtt':
    case "navigator.deviceMemory":
    case "navigator.devicePosture.type":
    case "navigator.gpu.wgslLanguageFeatures.size":
    case "navigator.hardwareConcurrency":
    case "navigator.language":
    case "navigator.onLine":
    case "navigator.platform":
    case "navigator.product":
    case "navigator.productSub":
    case "navigator.userAgentData.brands[0].brand":
    case "navigator.userAgentData.brands[0].version":
    case "navigator.vendor":
    case "screen.orientation.type":
        expected = "window." + propertyPath;
        break;

    // Whether or not an execution context is secure should not be affected by
    // detachment.
    case "isSecureContext":
        expected = "true";
        break;

    // The Cross-Origin-Embedder-Policy default value is 'unsafe-none'.
    case "crossOriginEmbedderPolicy":
        expected = "'unsafe-none'";
        break;

    // location's url is left intact on detach. The location getters will
    // provide the appropriate components of our test url (about:blank).
    case "location.href":
        expected = "'about:blank'";
        break;
    case "location.origin":
        expected = "'null'";
        break;
    case "location.pathname":
        expected = "'blank'";
        break;
    case "location.protocol":
        expected = "'about:'";
        break;

    case "navigator.mediaSession.playbackState":
        expected = "'none'";
        break;
    // Web tests are loaded from the local filesystem.
    case "origin":
        expected = "'file://'";
        break;
    }

    insertExpectedResult(path, expected);
}

function collectPropertiesHelper(global, object, path)
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
                collectPropertiesHelper(global, object[property], path);
            }
        } else if (type == "string") {
            emitExpectedResult(path, "''");
        } else if (type == "number") {
            emitExpectedResult(path, "0");
        } else if (type == "boolean") {
            emitExpectedResult(path, "false");
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
