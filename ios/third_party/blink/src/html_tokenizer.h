/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2010 Google, Inc. All Rights Reserved.
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

#ifndef HTMLTokenizer_h
#define HTMLTokenizer_h

#include "ios/third_party/blink/src/html_input_stream_preprocessor.h"
#include "ios/third_party/blink/src/html_token.h"

namespace WebCore {

class HTMLTokenizer {
public:
    HTMLTokenizer();

    HTMLTokenizer(const HTMLTokenizer&) = delete;
    HTMLTokenizer& operator=(const HTMLTokenizer&) = delete;

    ~HTMLTokenizer();

    void reset();

    enum State {
        DataState,
        TagOpenState,
        EndTagOpenState,
        TagNameState,
        BeforeAttributeNameState,
        AttributeNameState,
        AfterAttributeNameState,
        BeforeAttributeValueState,
        AttributeValueDoubleQuotedState,
        AttributeValueSingleQuotedState,
        AttributeValueUnquotedState,
        AfterAttributeValueQuotedState,
        SelfClosingStartTagState,
        BogusCommentState,
        // The ContinueBogusCommentState is not in the HTML5 spec, but we use
        // it internally to keep track of whether we've started the bogus
        // comment token yet.
        ContinueBogusCommentState,
        MarkupDeclarationOpenState,
        CommentStartState,
        CommentStartDashState,
        CommentState,
        CommentEndDashState,
        CommentEndState,
        CommentEndBangState,
        DOCTYPEState,
        BeforeDOCTYPENameState,
        DOCTYPENameState,
        AfterDOCTYPENameState,
        AfterDOCTYPEPublicKeywordState,
        BeforeDOCTYPEPublicIdentifierState,
        DOCTYPEPublicIdentifierDoubleQuotedState,
        DOCTYPEPublicIdentifierSingleQuotedState,
        AfterDOCTYPEPublicIdentifierState,
        BetweenDOCTYPEPublicAndSystemIdentifiersState,
        AfterDOCTYPESystemKeywordState,
        BeforeDOCTYPESystemIdentifierState,
        DOCTYPESystemIdentifierDoubleQuotedState,
        DOCTYPESystemIdentifierSingleQuotedState,
        AfterDOCTYPESystemIdentifierState,
        BogusDOCTYPEState,
        CDATASectionState,
        // These CDATA states are not in the HTML5 spec, but we use them internally.
        CDATASectionRightSquareBracketState,
        CDATASectionDoubleRightSquareBracketState,
    };

    // This function returns true if it emits a token. Otherwise, callers
    // must provide the same (in progress) token on the next call (unless
    // they call reset() first).
    bool nextToken(CharacterProvider&, HTMLToken&);

    State state() const { return m_state; }
    void setState(State state) { m_state = state; }

    inline bool shouldSkipNullCharacters() const
    {
        return m_state == HTMLTokenizer::DataState;
    }

private:
    inline void parseError();

    inline bool emitAndResumeIn(CharacterProvider& source, State state)
    {
        ASSERT(m_token->type() != HTMLToken::Uninitialized);
        m_state = state;
        source.next();
        return true;
    }

    inline bool emitAndReconsumeIn(CharacterProvider&, State state)
    {
        ASSERT(m_token->type() != HTMLToken::Uninitialized);
        m_state = state;
        return true;
    }

    inline bool emitEndOfFile(CharacterProvider& source)
    {
        if (haveBufferedCharacterToken())
            return true;
        m_state = HTMLTokenizer::DataState;
        source.next();
        m_token->clear();
        m_token->makeEndOfFile();
        return true;
    }

    // Return whether we need to emit a character token before dealing with
    // the buffered end tag.
    inline bool flushBufferedEndTag(CharacterProvider&);

    inline bool haveBufferedCharacterToken()
    {
        return m_token->type() == HTMLToken::Character;
    }

    State m_state;

    // m_token is owned by the caller. If nextToken is not on the stack,
    // this member might be pointing to unallocated memory.
    HTMLToken* m_token;

    // http://www.whatwg.org/specs/web-apps/current-work/#additional-allowed-character
    LChar m_additionalAllowedCharacter;

    // http://www.whatwg.org/specs/web-apps/current-work/#preprocessing-the-input-stream
    InputStreamPreprocessor<HTMLTokenizer> m_inputStreamPreprocessor;
};
}

#endif
