/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */
(function() {

// The MotionMark-based TextBenchmark should already be set to |window.benchmarkClass|
var TextBenchmark = window.benchmarkClass;
var TextTemplateBenchmark = Utilities.createSubclass(TextBenchmark,
    function(options)
    {
        var dataset;
        switch (options["corpus"]) {
        case "latin":
            dataset = [
                "σχέδιο",
                "umění",
                "suunnittelu",
                "design",
                "deseń",
                "искусство",
                "дизайн",
                "conception",
                "kunst",
                "konstruktion",
                "τέχνη",
                "diseño"
            ];
            break;
        case "cjk":
            dataset = [
                "设计",
                "디자인",
                "デザイン",
                "がいねん",
                "藝術",
                "养殖",
                "예술",
                "展開する",
                "발달",
                "技術",
                "驚き",
                "使吃惊",
            ];
            break;
        case "arabic":
            dataset = [
                {text: "تصميم", direction: "rtl"},
                "வடிவமைப்பு",
                "योजना",
                {text: "לְעַצֵב", direction: "rtl"},
                {text: "خلاق", direction: "rtl"},
                "ศิลปะ",
                "कौशल",
                {text: "אָמָנוּת", direction: "rtl"},
                "கலை",
                "ดีไซน์",
                "পরিকল্পনা",
                {text: "ډیزاین", direction: "rtl"},
            ];
            break;
        }

        dataset.forEach(function(entry, i) {
            var td = document.getElementById("cell" + i);
            if (typeof entry === 'string') {
                td.innerText = entry;
            } else {
                td.innerText = entry.text;
                td.classList.add("rtl");
            }
        })

        TextBenchmark.call(this, options);
    }
);

window.benchmarkClass = TextTemplateBenchmark;

})();