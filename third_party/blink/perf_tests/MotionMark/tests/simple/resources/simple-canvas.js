/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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
Utilities.extendObject(SimpleCanvasStage.prototype, {
    tune: function(count)
    {
        if (count == 0)
            return;

        if (count < 0) {
            this.offsetIndex = Math.max(this.offsetIndex + count, 0);
            return;
        }

        this.offsetIndex = this.offsetIndex + count;
        if (this.offsetIndex > this.objects.length) {
            // For some tests, it may be easier to see how well the test is going
            // by limiting the range of coordinates in which new objects can reside
            var coordinateMaximumFactor = Math.min(this.objects.length, Math.min(this.size.x, this.size.y)) / Math.min(this.size.x, this.size.y);
            var newIndex = this.offsetIndex - this.objects.length;
            for (var i = 0; i < newIndex; ++i)
                this.objects.push(new this._canvasObject(this, coordinateMaximumFactor));
        }
    },

    animate: function()
    {
        var context = this.context;
        context.clearRect(0, 0, this.size.x, this.size.y);
        for (var i = 0, length = this.offsetIndex; i < length; ++i)
            this.objects[i].draw(context);
    },

    complexity: function()
    {
        return this.offsetIndex;
    }
});
