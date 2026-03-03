function listGetComputedStyle(target) {
    // These properties have platform dependent values so we ignore them for convenience.
    var excludedProperties = new Set([
        '-webkit-tap-highlight-color',
        'font-family',
    ]);
    var properties = [
        // These properties don't show up when iterating a computed style object so we add them explicitly.
        "background-position-x",
        "background-position-y",
        "border-spacing",
        "overflow",
    ];
    var style = getComputedStyle(target);
    for (var i = 0; i < style.length; i++) {
        var property = style.item(i);
        if (!excludedProperties.has(property))
            properties.push(property);
    }
    for (var property of properties.sort())
        debug(property + ': ' + style[property]);
}
