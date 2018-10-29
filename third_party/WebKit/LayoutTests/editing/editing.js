//-------------------------------------------------------------------------------------------------------
// Java script library to run editing layout tests

var commandCount = 1;
var commandDelay = window.location.search.substring(1);
if (commandDelay == '')
    commandDelay = 0;
var selection = window.getSelection();

//-------------------------------------------------------------------------------------------------------

function execSetSelectionCommand(sn, so, en, eo) {
    window.getSelection().setBaseAndExtent(sn, so, en, eo);
}

// Args are startNode, startOffset, endNode, endOffset
function setSelectionCommand(sn, so, en, eo) {
    if (commandDelay > 0) {
        queueCommand(execSetSelectionCommand.bind(execSetSelectionCommand, sn, so, en, eo), commandCount * commandDelay);
        commandCount++;
    } else
        execSetSelectionCommand(sn, so, en, eo);
}

//-------------------------------------------------------------------------------------------------------

function execTransposeCharactersCommand() {
    document.execCommand("Transpose");
}
function transposeCharactersCommand() {
    if (commandDelay > 0) {
        queueCommand(execTransposeCharactersCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execTransposeCharactersCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionLeftByCharacterCommand() {
    selection.modify("move", "left", "character");
}
function moveSelectionLeftByCharacterCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionLeftByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionLeftByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionRightByCharacterCommand() {
    selection.modify("move", "Right", "character");
}
function moveSelectionRightByCharacterCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionRightByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionRightByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionLeftByCharacterCommand() {
    selection.modify("extend", "left", "character");
}
function extendSelectionLeftByCharacterCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionLeftByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionLeftByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionRightByCharacterCommand() {
    selection.modify("extend", "Right", "character");
}
function extendSelectionRightByCharacterCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionRightByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionRightByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionForwardByCharacterCommand() {
    selection.modify("move", "forward", "character");
}
function moveSelectionForwardByCharacterCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionForwardByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionForwardByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionForwardByCharacterCommand() {
    selection.modify("extend", "forward", "character");
}
function extendSelectionForwardByCharacterCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionForwardByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionForwardByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionForwardByWordCommand() {
    selection.modify("move", "forward", "word");
}
function moveSelectionForwardByWordCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionForwardByWordCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionForwardByWordCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionForwardByWordCommand() {
    selection.modify("extend", "forward", "word");
}
function extendSelectionForwardByWordCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionForwardByWordCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionForwardByWordCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionForwardBySentenceCommand() {
    selection.modify("move", "forward", "sentence");
}
function moveSelectionForwardBySentenceCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionForwardBySentenceCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionForwardBySentenceCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionForwardBySentenceCommand() {
    selection.modify("extend", "forward", "sentence");
}
function extendSelectionForwardBySentenceCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionForwardBySentenceCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionForwardBySentenceCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionForwardByLineCommand() {
    selection.modify("move", "forward", "line");
}
function moveSelectionForwardByLineCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionForwardByLineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionForwardByLineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionForwardByLineCommand() {
    selection.modify("extend", "forward", "line");
}
function extendSelectionForwardByLineCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionForwardByLineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionForwardByLineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionForwardByLineBoundaryCommand() {
    selection.modify("move", "forward", "lineBoundary");
}
function moveSelectionForwardByLineBoundaryCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionForwardByLineBoundaryCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionForwardByLineBoundaryCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionForwardByLineBoundaryCommand() {
    selection.modify("extend", "forward", "lineBoundary");
}
function extendSelectionForwardByLineBoundaryCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionForwardByLineBoundaryCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionForwardByLineBoundaryCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionBackwardByCharacterCommand() {
    selection.modify("move", "backward", "character");
}
function moveSelectionBackwardByCharacterCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionBackwardByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionBackwardByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionBackwardByCharacterCommand() {
    selection.modify("extend", "backward", "character");
}
function extendSelectionBackwardByCharacterCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionBackwardByCharacterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionBackwardByCharacterCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionBackwardByWordCommand() {
    selection.modify("move", "backward", "word");
}
function moveSelectionBackwardByWordCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionBackwardByWordCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionBackwardByWordCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionBackwardByWordCommand() {
    selection.modify("extend", "backward", "word");
}
function extendSelectionBackwardByWordCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionBackwardByWordCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionBackwardByWordCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionBackwardBySentenceCommand() {
    selection.modify("move", "backward", "sentence");
}
function moveSelectionBackwardBySentenceCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionBackwardBySentenceCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionBackwardBySentenceCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionBackwardBySentenceCommand() {
    selection.modify("extend", "backward", "sentence");
}
function extendSelectionBackwardBySentenceCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionBackwardBySentenceCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionBackwardBySentenceCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionBackwardByLineCommand() {
    selection.modify("move", "backward", "line");
}
function moveSelectionBackwardByLineCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionBackwardByLineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionBackwardByLineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionBackwardByLineCommand() {
    selection.modify("extend", "backward", "line");
}
function extendSelectionBackwardByLineCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionBackwardByLineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionBackwardByLineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execExtendSelectionBackwardByLineBoundaryCommand() {
    selection.modify("extend", "backward", "lineBoundary");
}
function extendSelectionBackwardByLineBoundaryCommand() {
    if (commandDelay > 0) {
        queueCommand(execExtendSelectionBackwardByLineBoundaryCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execExtendSelectionBackwardByLineBoundaryCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execMoveSelectionBackwardByLineBoundaryCommand() {
    selection.modify("move", "backward", "lineBoundary");
}
function moveSelectionBackwardByLineBoundaryCommand() {
    if (commandDelay > 0) {
        queueCommand(execMoveSelectionBackwardByLineBoundaryCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execMoveSelectionBackwardByLineBoundaryCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function doubleClick(x, y) {
    eventSender.mouseMoveTo(x, y);
    eventSender.mouseDown();
    eventSender.mouseUp();
    eventSender.mouseDown();
    eventSender.mouseUp();
}

function doubleClickAtSelectionStart() {
    var rects = window.getSelection().getRangeAt(0).getClientRects();
    var x = rects[0].left;
    var y = rects[0].top;
    doubleClick(x, y);
}

//-------------------------------------------------------------------------------------------------------

function execBoldCommand() {
    document.execCommand("Bold");
    debugForDumpAsText("execBoldCommand");
}
function boldCommand() {
    if (commandDelay > 0) {
        queueCommand(execBoldCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execBoldCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execUnderlineCommand() {
    document.execCommand("Underline");
    debugForDumpAsText("execUnderlineCommand");
}
function underlineCommand() {
    if (commandDelay > 0) {
        queueCommand(execUnderlineCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execUnderlineCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execFontNameCommand() {
    document.execCommand("FontName", false, "Courier");
    debugForDumpAsText("execFontNameCommand");
}
function fontNameCommand() {
    if (commandDelay > 0) {
        queueCommand(execFontNameCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execFontNameCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execFontSizeCommand(s) {
    if (arguments.length == 0 || s == undefined || s.length == 0)
        s = '12px';
    document.execCommand("FontSize", false, s);
    debugForDumpAsText("execFontSizeCommand");
}
function fontSizeCommand(s) {
    if (commandDelay > 0) {
        queueCommand(execFontSizeCommand, commandCount * commandDelay, s);
        commandCount++;
    }
    else {
        execFontSizeCommand(s);
    }
}

//-------------------------------------------------------------------------------------------------------

function execFontSizeDeltaCommand(s) {
    if (arguments.length == 0 || s == undefined || s.length == 0)
        s = '1px';
    document.execCommand("FontSizeDelta", false, s);
    debugForDumpAsText("execFontSizeDeltaCommand");
}
function fontSizeDeltaCommand(s) {
    if (commandDelay > 0) {
        queueCommand(execFontSizeDeltaCommand.bind(execFontSizeDeltaCommand, s), commandCount * commandDelay);
        commandCount++;
    }
    else {
        execFontSizeDeltaCommand(s);
    }
}

//-------------------------------------------------------------------------------------------------------

function execItalicCommand() {
    document.execCommand("Italic");
    debugForDumpAsText("execItalicCommand");
}
function italicCommand() {
    if (commandDelay > 0) {
        queueCommand(execItalicCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execItalicCommand();
    }
}


//-------------------------------------------------------------------------------------------------------

function execJustifyCenterCommand() {
    document.execCommand("JustifyCenter");
    debugForDumpAsText("execJustifyCenterCommand");
}
function justifyCenterCommand() {
    if (commandDelay > 0) {
        queueCommand(execJustifyCenterCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execJustifyCenterCommand();
    }
}


//-------------------------------------------------------------------------------------------------------

function execJustifyLeftCommand() {
    document.execCommand("JustifyLeft");
    debugForDumpAsText("execJustifyLeftCommand");
}
function justifyLeftCommand() {
    if (commandDelay > 0) {
        queueCommand(execJustifyLeftCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execJustifyLeftCommand();
    }
}


//-------------------------------------------------------------------------------------------------------

function execJustifyRightCommand() {
    document.execCommand("JustifyRight");
    debugForDumpAsText("execJustifyRightCommand");
}
function justifyRightCommand() {
    if (commandDelay > 0) {
        queueCommand(execJustifyRightCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execJustifyRightCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execInsertHTMLCommand(html) {
    document.execCommand("InsertHTML", false, html);
    debugForDumpAsText("execInsertHTMLCommand");
}
function insertHTMLCommand(html) {
    if (commandDelay > 0) {
        queueCommand(execInsertHTMLCommand.bind(execInsertHTMLCommand, html), commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertHTMLCommand(html);
    }
}

//-------------------------------------------------------------------------------------------------------

function execInsertImageCommand(imgSrc) {
    document.execCommand("InsertImage", false, imgSrc);
    debugForDumpAsText("execInsertImageCommand");
}
function insertImageCommand(imgSrc) {
    if (commandDelay > 0) {
        queueCommand(execInsertImageCommand.bind(execInsertImageCommand, imgSrc), commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertImageCommand(imgSrc);
    }
}

//-------------------------------------------------------------------------------------------------------

function execInsertLineBreakCommand() {
    document.execCommand("InsertLineBreak");
    debugForDumpAsText("execInsertLineBreakCommand");
}
function insertLineBreakCommand() {
    if (commandDelay > 0) {
        queueCommand(execInsertLineBreakCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertLineBreakCommand();
    }
}

//-------------------------------------------------------------------------------------------------------
 
function execInsertParagraphCommand() {
    document.execCommand("InsertParagraph");
    debugForDumpAsText("execInsertParagraphCommand");
}
function insertParagraphCommand() {
    if (commandDelay > 0) {
        queueCommand(execInsertParagraphCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertParagraphCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execInsertNewlineInQuotedContentCommand() {
    document.execCommand("InsertNewlineInQuotedContent");
    debugForDumpAsText("execInsertNewlineInQuotedContentCommand");
}
function insertNewlineInQuotedContentCommand() {
    if (commandDelay > 0) {
        queueCommand(execInsertNewlineInQuotedContentCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execInsertNewlineInQuotedContentCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execTypeCharacterCommand(c) {
    if (arguments.length == 0 || c == undefined || c.length == 0 || c.length > 1)
        c = 'x';
    document.execCommand("InsertText", false, c);
    debugForDumpAsText("execTypeCharacterCommand");
}
function typeCharacterCommand(c) {
    if (commandDelay > 0) {
        queueCommand(execTypeCharacterCommand.bind(execTypeCharacterCommand, c), commandCount * commandDelay);
        commandCount++;
    }
    else {
        execTypeCharacterCommand(c);
    }
}

//-------------------------------------------------------------------------------------------------------

function execSelectAllCommand() {
    document.execCommand("SelectAll");
}
function selectAllCommand() {
    if (commandDelay > 0) {
        queueCommand(execSelectAllCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execSelectAllCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execStrikethroughCommand() {
    document.execCommand("Strikethrough");
    debugForDumpAsText("execStrikethroughCommand");
}
function strikethroughCommand() {
    if (commandDelay > 0) {
        queueCommand(execStrikethroughCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execStrikethroughCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execUndoCommand() {
    document.execCommand("Undo");
    debugForDumpAsText("execUndoCommand");
}
function undoCommand() {
    if (commandDelay > 0) {
        queueCommand(execUndoCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execUndoCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execRedoCommand() {
    document.execCommand("Redo");
    debugForDumpAsText("execRedoCommand");
}
function redoCommand() {
    if (commandDelay > 0) {
        queueCommand(execRedoCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execRedoCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execChangeRootSize() {
    document.getElementById("root").style.width = "600px";
}
function changeRootSize() {
    if (commandDelay > 0) {
        queueCommand(execChangeRootSize, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execChangeRootSize();
    }
}

//-------------------------------------------------------------------------------------------------------

function execCutCommand() {
    document.execCommand("Cut");
    debugForDumpAsText("execCutCommand");
}
function cutCommand() {
    if (commandDelay > 0) {
        queueCommand(execCutCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execCutCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execCopyCommand() {
    document.execCommand("Copy");
    debugForDumpAsText("execCopyCommand");
}
function copyCommand() {
    if (commandDelay > 0) {
        queueCommand(execCopyCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execCopyCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execPasteCommand() {
    document.execCommand("Paste");
    debugForDumpAsText("execPasteCommand");
}
function pasteCommand() {
    if (commandDelay > 0) {
        queueCommand(execPasteCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execPasteCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execCreateLinkCommand(url) {
    document.execCommand("CreateLink", false, url);
    debugForDumpAsText("execCreateLinkCommand");
}
function createLinkCommand(url) {
    if (commandDelay > 0) {
        queueCommand(execCreateLinkCommand.bind(execCreateLinkCommand, url), commandCount * commandDelay);
        commandCount++;
    } else
        execCreateLinkCommand(url);
}

//-------------------------------------------------------------------------------------------------------

function execUnlinkCommand() {
    document.execCommand("Unlink");
    debugForDumpAsText("execUnlinkCommand");
}
function unlinkCommand() {
    if (commandDelay > 0) {
        queueCommand(execUnlinkCommand, commandCount * commandDelay);
        commandCount++;
    } else
        execUnlinkCommand();
}

//-------------------------------------------------------------------------------------------------------

function execPasteAndMatchStyleCommand() {
    document.execCommand("PasteAndMatchStyle");
    debugForDumpAsText("execPasteAndMatchStyleCommand");
}
function pasteAndMatchStyleCommand() {
    if (commandDelay > 0) {
        queueCommand(execPasteAndMatchStyleCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execPasteAndMatchStyleCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execDeleteCommand() {
    document.execCommand("Delete");
    debugForDumpAsText("execDeleteCommand");
}
function deleteCommand() {
    if (commandDelay > 0) {
        queueCommand(execDeleteCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execDeleteCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

function execForwardDeleteCommand() {
    document.execCommand("ForwardDelete");
    debugForDumpAsText("execForwardDeleteCommand");
}
function forwardDeleteCommand() {
    if (commandDelay > 0) {
        queueCommand(execForwardDeleteCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execForwardDeleteCommand();
    }
}

//-------------------------------------------------------------------------------------------------------

(function () {
    var queue = [];
    var i = 0;
    var timer;

    function queueCommand(callback) {
        queue.push(callback);
        if (!timer) {
            if (window.testRunner)
                testRunner.waitUntilDone();
            timer = setTimeout(runCommand, commandDelay);
        }
    }

    function runCommand() {
        queue[i]();
        i++;
        if (i < queue.length)
            setTimeout(runCommand, commandDelay);
        else if (window.testRunner)
            testRunner.notifyDone();
    }
    
    window.queueCommand = queueCommand;
})();

function focusOnFirstTextInTestElementIfExists() {
    var elem = document.getElementById("test");
    var selection = window.getSelection();
    if (elem) {
        var traverse = function (node, offset, condition) {
            var obj = condition(node, offset);
            if (obj)
                return obj;

            var children = node.childNodes;
            var len = children.length;
            for (var i = 0; i < len; i++) {
                var child = children[i];
                obj = traverse(child, i, condition);
                if (obj)
                    return obj;
            }
            return null;
        }

        var firstVisiblePosition = traverse(elem, 0, function (node, offset) {
            if (node.nodeName == '#text') {
                offset = 0;
                while (offset < node.textContent.length && node.textContent[offset] == '\n')
                    offset++;
                return { 'node': node, 'offset': offset };
            }

            if (node.nodeName == 'BR' || node.nodeName == 'IMG')
                return { 'node': node.parentNode, 'offset': offset };

            return null;
        });

        if (firstVisiblePosition)
            selection.collapse(firstVisiblePosition.node, firstVisiblePosition.offset);
        else
            selection.collapse(elem, 0);
    } else {
        selection.removeAllRanges();
    }
}

function runEditingTest() {
    if (window.testRunner) {
        testRunner.dumpEditingCallbacks();
        testRunner.dumpAsLayoutWithPixelResults();
    }

    focusOnFirstTextInTestElementIfExists();

    editingTest();
}

var dumpAsText = false;
var elementsForDumpingMarkupList = [document.createElement('ol')];

function runDumpAsTextEditingTest(enableCallbacks) {
    if (window.testRunner) {
         testRunner.dumpAsText();
         if (enableCallbacks)
            testRunner.dumpEditingCallbacks();
     }

    dumpAsText = true;

    focusOnFirstTextInTestElementIfExists();

    editingTest();

    for (var i = 0; i < elementsForDumpingMarkupList.length; i++)
        document.body.appendChild(elementsForDumpingMarkupList[i]);
}

function debugForDumpAsText(name) {
    if (dumpAsText && document.getElementById("root")) {
        var newItem = document.createElement('li');
        newItem.appendChild(document.createTextNode(name+": "+document.getElementById("root").innerHTML));
        elementsForDumpingMarkupList[elementsForDumpingMarkupList.length - 1].appendChild(newItem);
    }
}

function startNewMarkupGroup(label) {
    if (!elementsForDumpingMarkupList[elementsForDumpingMarkupList.length - 1].hasChildNodes())
        elementsForDumpingMarkupList.pop();
    elementsForDumpingMarkupList.push(document.createElement('br'));
    elementsForDumpingMarkupList.push(document.createTextNode(label));
    elementsForDumpingMarkupList.push(document.createElement('ol'));
}

//-------------------------------------------------------------------------------------------------------


function execBackColorCommand() {
    document.execCommand("BackColor", false, "Chartreuse");
    debugForDumpAsText('execBackColorCommand');
}
function backColorCommand() {
    if (commandDelay > 0) {
        queueCommand(execBackColorCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        execBackColorCommand();
    }
}


function execForeColorCommand(color) {
    document.execCommand("ForeColor", false, color);
    debugForDumpAsText('execForeColorCommand');
}
function foreColorCommand(color) {
    if (commandDelay > 0) {
        queueCommand(execForeColorCommand.bind(execForeColorCommand, color), commandCount * commandDelay);
        commandCount++;
    } else
        execForeColorCommand(color);
}

//-------------------------------------------------------------------------------------------------------


function runCommand(command, arg1, arg2) {
    document.execCommand(command,arg1,arg2);
}

function executeCommand(command,arg1,arg2) {
    if (commandDelay > 0) {
        queueCommand(runCommand, commandCount * commandDelay);
        commandCount++;
    }
    else {
        runCommand(command,arg1,arg2);
    }
}

