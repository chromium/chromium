function valueForEntry(element) {
    if (!element)
        return null;
    var value = element.dataset.value;
    if (typeof value === "string")
        return value;
    var action = element.dataset.action;
    if (typeof action === "string")
        return "@" + action;
    return null;
}

function highlightedEntry() {
    return valueForEntry(internals.pagePopupWindow.document.activeElement);
}

function entryValues() {
    var elements = internals.pagePopupWindow.document.getElementsByClassName("suggestion-list-entry");
    var values = [];
    for (var i = 0; i < elements.length; ++i)
        values.push(valueForEntry(elements[i]));
    return values;
}
