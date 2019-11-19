description("This test exercises the 'font' shorthand property in CSS computed styles.");

var testDiv = document.createElement('div');
document.body.appendChild(testDiv);

function computedFont(fontString) {
    testDiv.style.font = 'bold 600px serif';
    testDiv.style.font = fontString;
    return window.getComputedStyle(testDiv).getPropertyValue('font');
}

shouldBe("computedFont('10px sans-serif')", "'10px sans-serif'");
shouldBe("computedFont('10px sans-serif')", "'10px sans-serif'");
shouldBe("computedFont('10px SANS-SERIF')", "'10px sans-serif'");
shouldBe("computedFont('12px sans-serif')", "'12px sans-serif'");
shouldBe("computedFont('12px  sans-serif')", "'12px sans-serif'");
shouldBe("computedFont('10px sans-serif, sans-serif')", "'10px sans-serif, sans-serif'");
shouldBe("computedFont('10px sans-serif, serif')", "'10px sans-serif, serif'");
shouldBe("computedFont('12px ahem')", "'12px ahem'");
shouldBe("computedFont('12px unlikely-font-name')", "'12px unlikely-font-name'");
shouldBe("computedFont('100 10px sans-serif')", "'100 10px sans-serif'");
shouldBe("computedFont('200 10px sans-serif')", "'200 10px sans-serif'");
shouldBe("computedFont('300 10px sans-serif')", "'300 10px sans-serif'");
shouldBe("computedFont('400 10px sans-serif')", "'10px sans-serif'");
shouldBe("computedFont('normal 10px sans-serif')", "'10px sans-serif'");
shouldBe("computedFont('500 10px sans-serif')", "'500 10px sans-serif'");
shouldBe("computedFont('600 10px sans-serif')", "'600 10px sans-serif'");
shouldBe("computedFont('700 10px sans-serif')", "'700 10px sans-serif'");
shouldBe("computedFont('bold 10px sans-serif')", "'700 10px sans-serif'");
shouldBe("computedFont('800 10px sans-serif')", "'800 10px sans-serif'");
shouldBe("computedFont('900 10px sans-serif')", "'900 10px sans-serif'");
shouldBe("computedFont('italic 10px sans-serif')", "'italic 10px sans-serif'");
shouldBe("computedFont('small-caps 10px sans-serif')", "'small-caps 10px sans-serif'");
shouldBe("computedFont('italic small-caps 10px sans-serif')", "'italic small-caps 10px sans-serif'");
shouldBe("computedFont('italic small-caps bold 10px sans-serif')", "'italic small-caps 700 10px sans-serif'");
shouldBe("computedFont('10px/100% sans-serif')", "'10px / 10px sans-serif'");
shouldBe("computedFont('10px/100px sans-serif')", "'10px / 100px sans-serif'");
shouldBe("computedFont('10px/normal sans-serif')", "'10px sans-serif'");
shouldBe("computedFont('10px/normal sans-serif')", "'10px sans-serif'");
