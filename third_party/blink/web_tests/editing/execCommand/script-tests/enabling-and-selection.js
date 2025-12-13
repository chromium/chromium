description("This tests enabling of execCommand commands based on whether the selection is a caret or range or in editable content.");

var nonEditableParagraph = document.createElement("p");
nonEditableParagraph.appendChild(document.createTextNode("x"));
document.body.appendChild(nonEditableParagraph);

var editableParagraph = document.createElement("p");
editableParagraph.appendChild(document.createTextNode("x"));
editableParagraph.setAttribute("contentEditable", "true");
document.body.appendChild(editableParagraph);

var editablePlainTextParagraph = document.createElement("p");
editablePlainTextParagraph.appendChild(document.createTextNode("x"));
editablePlainTextParagraph.setAttribute("contentEditable", "plaintext-only");
document.body.appendChild(editablePlainTextParagraph);

function enabled(command, element, selectionStart, selectionEnd)
{
    var selection = document.getSelection();
    selection.removeAllRanges();
    if (element) {
        var range = document.createRange();
        range.setStart(element.firstChild, selectionStart);
        range.setEnd(element.firstChild, selectionEnd);
        selection.addRange(range);
    }
    var result = document.queryCommandEnabled(command)
    selection.removeAllRanges();
    return result;
}

function whenEnabled(command)
{
    var enabledWithNoSelection = enabled(command);
    var enabledWithCaret = enabled(command, editableParagraph, 0, 0);
    var enabledWithEditableRange = enabled(command, editableParagraph, 0, 1);
    var enabledWithPlainTextCaret = enabled(command, editablePlainTextParagraph, 0, 0);
    var enabledWithPlainTextEditableRange = enabled(command, editablePlainTextParagraph, 0, 1);
    var enabledWithPoint = enabled(command, nonEditableParagraph, 0, 0);
    var enabledWithNonEditableRange = enabled(command, nonEditableParagraph, 0, 1);

    var summaryInteger = enabledWithNoSelection
        | (enabledWithCaret << 1)
        | (enabledWithEditableRange << 2)
        | (enabledWithPlainTextCaret << 3)
        | (enabledWithPlainTextEditableRange << 4)
        | (enabledWithPoint << 5)
        | (enabledWithNonEditableRange << 6);

    if (summaryInteger === 0x7F)
        return "always";

    if (summaryInteger === 0x54)
        return "range";

    if (summaryInteger === 0x1E)
        return "editable";
    if (summaryInteger === 0x0A)
        return "caret";
    if (summaryInteger === 0x14)
        return "editable range";

    if (summaryInteger === 0x06)
        return "richly editable";
    if (summaryInteger === 0x02)
        return "richly editable caret";
    if (summaryInteger === 0x04)
        return "richly editable range";

    if (summaryInteger === 0x5E)
        return "visible";

    return summaryInteger;
}

shouldBe("whenEnabled('FindString')", "'always'");
shouldBe("whenEnabled('Print')", "'always'");
shouldBe("whenEnabled('SelectAll')", "'always'");

shouldBe("whenEnabled('Transpose')", "'caret'");

shouldBe("whenEnabled('Copy')", "'range'");

shouldBe("whenEnabled('Cut')", "'editable range'");
shouldBe("whenEnabled('RemoveFormat')", "'richly editable range'");

shouldBe("whenEnabled('Delete')", "'editable'");
shouldBe("whenEnabled('FontName')", "'richly editable'");
shouldBe("whenEnabled('FontSize')", "'richly editable'");
shouldBe("whenEnabled('FontSizeDelta')", "'richly editable'");
shouldBe("whenEnabled('ForwardDelete')", "'editable'");
shouldBe("whenEnabled('InsertHTML')", "'editable'");
shouldBe("whenEnabled('InsertParagraph')", "'editable'");
shouldBe("whenEnabled('InsertText')", "'editable'");

shouldBe("whenEnabled('BackColor')", "'richly editable'");
shouldBe("whenEnabled('Bold')", "'richly editable'");
shouldBe("whenEnabled('CreateLink')", "'richly editable'");
shouldBe("whenEnabled('ForeColor')", "'richly editable'");
shouldBe("whenEnabled('FormatBlock')", "'richly editable'");
shouldBe("whenEnabled('HiliteColor')", "'richly editable'");
shouldBe("whenEnabled('Indent')", "'richly editable'");
shouldBe("whenEnabled('InsertHorizontalRule')", "'richly editable'");
shouldBe("whenEnabled('InsertImage')", "'richly editable'");
shouldBe("whenEnabled('InsertNewlineInQuotedContent')", "'richly editable'");
shouldBe("whenEnabled('InsertOrderedList')", "'richly editable'");
shouldBe("whenEnabled('InsertUnorderedList')", "'richly editable'");
shouldBe("whenEnabled('Italic')", "'richly editable'");
shouldBe("whenEnabled('JustifyCenter')", "'richly editable'");
shouldBe("whenEnabled('JustifyFull')", "'richly editable'");
shouldBe("whenEnabled('JustifyLeft')", "'richly editable'");
shouldBe("whenEnabled('JustifyNone')", "'richly editable'");
shouldBe("whenEnabled('JustifyRight')", "'richly editable'");
shouldBe("whenEnabled('Outdent')", "'richly editable'");
shouldBe("whenEnabled('Strikethrough')", "'richly editable'");
shouldBe("whenEnabled('Subscript')", "'richly editable'");
shouldBe("whenEnabled('Superscript')", "'richly editable'");
shouldBe("whenEnabled('Underline')", "'richly editable'");

shouldBe("whenEnabled('Paste')", "'editable'");
shouldBe("whenEnabled('PasteAndMatchStyle')", "'editable'");

shouldBe("whenEnabled('Unlink')", "'richly editable range'");

shouldBe("whenEnabled('Unselect')", "'visible'");

document.body.removeChild(nonEditableParagraph);
document.body.removeChild(editableParagraph);
document.body.removeChild(editablePlainTextParagraph);

var successfullyParsed = true;
