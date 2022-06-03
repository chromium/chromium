function $(id) {
    return document.getElementById(id);
}

function createFormControlDataSet() {
    // A list of labelable elements resides in http://www.whatwg.org/specs/web-apps/current-work/multipage/forms.html#category-label
    var formControlClassNames = [
        'HTMLButtonElement',
        'HTMLDataListElement',
        'HTMLFieldSetElement',
        'HTMLInputElement',
        'HTMLLabelElement',
        'HTMLLegendElement',
        'HTMLMeterElement',
        'HTMLObjectElement',
        'HTMLOptGroupElement',
        'HTMLOptionElement',
        'HTMLOutputElement',
        'HTMLProgressElement',
        'HTMLSelectElement',
        'HTMLTextAreaElement'
    ];
    var formControlDataSet = {};
    for (var i = 0; i < formControlClassNames.length; i++) {
        var className = formControlClassNames[i];
        var tagName = className.toLowerCase().substring(4, className.length - 7);
        var element = document.createElement(tagName);
        formControlDataSet[tagName] = {
            inputType: null,
            isLabelable: true,
            isSupported: element.toString() == '[object ' + className + ']',
            name: tagName,
            tagName: tagName,
        };
    }
    formControlDataSet.datalist.isLabelable = false;
    formControlDataSet.fieldset.isLabelable = false;
    formControlDataSet.label.isLabelable = false;
    formControlDataSet.legend.isLabelable = false;
    formControlDataSet.object.isLabelable = false;
    formControlDataSet.optgroup.isLabelable = false;
    formControlDataSet.option.isLabelable = false;

    // Input type names reside in http://www.whatwg.org/specs/web-apps/current-work/multipage/the-input-element.html
    var inputTypeNames = [
        'button',
        'checkbox',
        'color',
        'date',
        'datetime',
        'datetime-local',
        'email',
        'file',
        'hidden',
        'image',
        'month',
        'number',
        'password',
        'radio',
        'range',
        'reset',
        'search',
        'submit',
        'tel',
        'text',
        'time',
        'url',
        'week',
    ];
    for (var i = 0; i < inputTypeNames.length; i++) {
        var typeName = inputTypeNames[i];
        var name = typeName + 'Type';
        var element = document.createElement('input');
        element.type = typeName;
        formControlDataSet[name] = {
            inputType: typeName,
            isLabelable: true,
            isSupported: element.type == typeName,
            name: name,
            tagName: 'input',
      };
    }
    formControlDataSet.hiddenType.isLabelable = false;

    return formControlDataSet;
}

function getAbsoluteRect(element) {
    var rect = element.getBoundingClientRect();
    rect.top += document.scrollingElement.scrollTop;
    rect.bottom += document.scrollingElement.scrollTop;
    rect.left += document.scrollingElement.scrollLeft;
    rect.right += document.scrollingElement.scrollLeft;
    return rect;
}

function searchCancelButtonPosition(element) {
    var offset = cumulativeOffset(element);
    var pos = {};
    pos.x = offset[0] + element.offsetWidth - 9;
    pos.y = offset[1] + element.offsetHeight / 2;
    return pos;
}

function rtlSearchCancelButtonPosition(element) {
    var offset = cumulativeOffset(element);
    var pos = {};
    pos.x = offset[0] + 9;
    pos.y = offset[1] + element.offsetHeight / 2;
    return pos;
}

function mouseMoveToIndexInListbox(index, listboxId) {
    var listbox = document.getElementById(listboxId);
    var itemHeight = Math.floor(listbox.offsetHeight / listbox.size);
    var border = 1;
    var y = border + index * itemHeight;
    if (window.eventSender)
        eventSender.mouseMoveTo(listbox.offsetLeft + border, listbox.offsetTop + y - window.pageYOffset);
}

function getUserAgentShadowTextContent(element) {
    return internals.shadowRoot(element).textContent;
};

function cumulativeOffset(element) {
    var x = 0;
    var y = 0;
    var parentFrame = element.ownerDocument.defaultView.frameElement;
    if (parentFrame) {
        var parentFrameOffset = cumulativeOffset(parentFrame);
        x = parentFrameOffset[0];
        y = parentFrameOffset[1];
    }
    if (element.parentNode) {
        do {
            x += element.offsetLeft || 0;
            y += element.offsetTop  || 0;
            element = element.offsetParent;
        } while (element);
    }
    return [x, y];
}

function elementCenterPosition(element) {
    var offset = cumulativeOffset(element);
    var centerX = offset[0] + element.offsetWidth / 2;
    var centerY = offset[1] + element.offsetHeight / 2;
    return [centerX, centerY];
}

function hoverOverElement(element) {
    var center = elementCenterPosition(element);
    eventSender.mouseMoveTo(center[0], center[1]);
}

function clickElement(element) {
    hoverOverElement(element);
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function pressElement(element) {
    hoverOverElement(element);
    eventSender.mouseDown();
}

function traverseNextNode(node, stayWithin) {
    var nextNode = node.firstChild;
    if (nextNode)
        return nextNode;

    if (stayWithin && node === stayWithin)
        return null;

    nextNode = node.nextSibling;
    if (nextNode)
        return nextNode;

    nextNode = node;
    while (nextNode && !nextNode.nextSibling && (!stayWithin || !nextNode.parentNode || nextNode.parentNode !== stayWithin))
        nextNode = nextNode.parentNode;
    if (!nextNode)
        return null;

    return nextNode.nextSibling;
}

function getElementByPseudoId(root, pseudoId) {
    if (!window.internals)
        return null;
    var node = root;
    while (node) {
        if (node.nodeType === Node.ELEMENT_NODE && internals.shadowPseudoId(node) === pseudoId)
            return node;
        node = traverseNextNode(node, root);
    }
    return null;
}

function doneLater() {
    setTimeout(function() {
        testRunner.notifyDone();
    }, 0);
}

function waitUntilLoadedAndAutofocused(callback) {
    var loaded = false;
    var autofocused = false;
    // Use doneLater() because some rendering tests need repaint after focus.
    callback  = callback || doneLater;
    // Does both of waitUntilDone and jsTestIsAsync because we want to support
    // tests with/without js-test.js.
    testRunner.waitUntilDone();
    window.jsTestIsAsync = true;
    window.addEventListener('load', function() {
        loaded = true;
        if (autofocused)
            callback();
    }, false);
    document.addEventListener('focusin', function() {
        if (internals.hasAutofocusRequest(document))
            return;
        if (autofocused)
            return;
        autofocused = true;
        if (loaded)
            callback();
    }, false);
}

function sendString(str) {
    if (!window.eventSender) {
        console.log('Require eventSender.');
        return;
    }
    for (var i = 0; i < str.length; ++i) {
        var key = str.charAt(i);
        if (key == '\n')
            key = 'Enter';
        eventSender.keyDown(key);
    }
}

