/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HTMLToken_h
#define HTMLToken_h

#include <stddef.h>
#include <vector>

#include "ios/third_party/blink/src/html_tokenizer_adapter.h"

namespace WebCore {

class HTMLToken {
public:
    enum Type {
        Uninitialized,
        DOCTYPE,
        StartTag,
        EndTag,
        Comment,
        Character,
        EndOfFile,
    };

    HTMLToken();

    HTMLToken(const HTMLToken&) = delete;
    HTMLToken& operator=(const HTMLToken&) = delete;

    ~HTMLToken();

    void clear()
    {
        m_type = Uninitialized;
        m_data.clear();
    }

    Type type() const { return m_type; }

    void makeEndOfFile()
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndOfFile;
    }

    void appendToName(LChar character)
    {
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        ASSERT(character);
        m_data.push_back(character);
    }

    bool nameEquals(const LChar* name, size_t length)
    {
        ASSERT(m_type == StartTag || m_type == EndTag || m_type == DOCTYPE);
        if (length != m_data.size())
            return false;

        for (size_t index = 0; index < length; ++index) {
            if (m_data.at(index) != name[index])
                return false;
        }

        return true;
    }

    /* DOCTYPE Tokens */

    void beginDOCTYPE()
    {
        ASSERT(m_type == Uninitialized);
        m_type = DOCTYPE;
    }

    /* Start/End Tag Tokens */

    void beginStartTag(LChar character)
    {
        ASSERT(character);
        ASSERT(m_type == Uninitialized);
        m_type = StartTag;

        m_data.push_back(character);
    }

    void beginEndTag(LChar character)
    {
        ASSERT(m_type == Uninitialized);
        m_type = EndTag;

        m_data.push_back(character);
    }

    /* Character Tokens */

    // Starting a character token works slightly differently than starting
    // other types of tokens because we want to save a per-character branch.
    void ensureIsCharacterToken()
    {
        ASSERT(m_type == Uninitialized || m_type == Character);
        m_type = Character;
    }

    /* Comment Tokens */

    void beginComment()
    {
        ASSERT(m_type == Uninitialized);
        m_type = Comment;
    }

private:
    Type m_type;
    std::vector<LChar> m_data;
};
}

#endif
