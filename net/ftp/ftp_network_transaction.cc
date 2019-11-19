// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/ftp/ftp_network_transaction.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback_helpers.h"
#include "base/compiler_specific.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/address_list.h"
#include "net/base/completion_once_callback.h"
#include "net/base/escape.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/network_isolation_key.h"
#include "net/base/parse_number.h"
#include "net/base/port_util.h"
#include "net/base/url_util.h"
#include "net/ftp/ftp_request_info.h"
#include "net/ftp/ftp_util.h"
#include "net/log/net_log.h"
#include "net/log/net_log_event_type.h"
#include "net/log/net_log_source.h"
#include "net/socket/client_socket_factory.h"
#include "net/socket/stream_socket.h"
#include "url/url_constants.h"

namespace net {

namespace {

const char kCRLF[] = "\r\n";

const int kCtrlBufLen = 1024;

// Returns true if |input| can be safely used as a part of an FTP command.
bool IsValidFTPCommandSubstring(const std::string& input) {
  // RFC 959 only allows ASCII strings, but at least Firefox can send non-ASCII
  // characters in the command if the request path contains them. To be
  // compatible, we do the same and allow non-ASCII characters in a command.

  // Protect agains newline injection attack.
  if (input.find_first_of("\r\n") != std::string::npos)
    return false;

  return true;
}

enum ErrorClass {
  // The requested action was initiated. The client should expect another
  // reply before issuing the next command.
  ERROR_CLASS_INITIATED,

  // The requested action has been successfully completed.
  ERROR_CLASS_OK,

  // The command has been accepted, but to complete the operation, more
  // information must be sent by the client.
  ERROR_CLASS_INFO_NEEDED,

  // The command was not accepted and the requested action did not take place.
  // This condition is temporary, and the client is encouraged to restart the
  // command sequence.
  ERROR_CLASS_TRANSIENT_ERROR,

  // The command was not accepted and the requested action did not take place.
  // This condition is rather permanent, and the client is discouraged from
  // repeating the exact request.
  ERROR_CLASS_PERMANENT_ERROR,
};

// Returns the error class for given response code. Caller should ensure
// that |response_code| is in range 100-599.
ErrorClass GetErrorClass(int response_code) {
  if (response_code >= 100 && response_code <= 199)
    return ERROR_CLASS_INITIATED;

  if (response_code >= 200 && response_code <= 299)
    return ERROR_CLASS_OK;

  if (response_code >= 300 && response_code <= 399)
    return ERROR_CLASS_INFO_NEEDED;

  if (response_code >= 400 && response_code <= 499)
    return ERROR_CLASS_TRANSIENT_ERROR;

  if (response_code >= 500 && response_code <= 599)
    return ERROR_CLASS_PERMANENT_ERROR;

  // We should not be called on invalid error codes.
  NOTREACHED() << response_code;
  return ERROR_CLASS_PERMANENT_ERROR;
}

// Returns network error code for received FTP |response_code|.
int GetNetErrorCodeForFtpResponseCode(int response_code) {
  switch (response_code) {
    case 421:
      return ERR_FTP_SERVICE_UNAVAILABLE;
    case 426:
      return ERR_FTP_TRANSFER_ABORTED;
    case 450:
      return ERR_FTP_FILE_BUSY;
    case 500:
    case 501:
      return ERR_FTP_SYNTAX_ERROR;
    case 502:
    case 504:
      return ERR_FTP_COMMAND_NOT_SUPPORTED;
    case 503:
      return ERR_FTP_BAD_COMMAND_SEQUENCE;
    default:
      return ERR_FTP_FAILED;
  }
}

// From RFC 2428 Section 3:
//   The text returned in response to the EPSV command MUST be:
//     <some text> (<d><d><d><tcp-port><d>)
//   <d> is a delimiter character, ideally to be |
bool ExtractPortFromEPSVResponse(const FtpCtrlResponse& response, int* port) {
  if (response.lines.size() != 1)
    return false;

  base::StringPiece epsv_line(response.lines[0]);
  size_t start = epsv_line.find('(');
  // If the line doesn't have a '(' or doesn't have enough characters after the
  // first '(', fail.
  if (start == base::StringPiece::npos || epsv_line.length() - start < 7)
    return false;

  char separator = epsv_line[start + 1];

  // Make sure we have "(<d><d><d>...", where <d> is not a number.
  if ((separator >= '0' && separator <= '9') ||
      epsv_line[start + 2] != separator || epsv_line[start + 3] != separator) {
    return false;
  }

  // Skip over those characters.
  start += 4;

  // Make sure there's a terminal <d>.
  size_t end = epsv_line.find(separator, start);
  if (end == base::StringPiece::npos)
    return false;

  return ParseInt32(epsv_line.substr(start, end - start),
                    ParseIntFormat::NON_NEGATIVE, port);
}

// There are two way we can receive IP address and port.
// (127,0,0,1,23,21) IP address and port encapsulated in ().
// 127,0,0,1,23,21  IP address and port without ().
//
// See RFC 959, Section 4.1.2
bool ExtractPortFromPASVResponse(const FtpCtrlResponse& response, int* port) {
  if (response.lines.size() != 1)
    return false;

  std::string line(response.lines[0]);
  if (!base::IsStringASCII(line))
    return false;
  if (line.length() < 2)
    return false;

  size_t paren_pos = line.find('(');
  if (paren_pos == std::string::npos) {
    // Find the first comma and use it to locate the beginning
    // of the response data.
    size_t comma_pos = line.find(',');
    if (comma_pos == std::string::npos)
      return false;

    size_t space_pos = line.rfind(' ', comma_pos);
    if (space_pos != std::string::npos)
      line = line.substr(space_pos + 1);
  } else {
    // Remove the parentheses and use the text inside them.
    size_t closing_paren_pos = line.rfind(')');
    if (closing_paren_pos == std::string::npos)
      return false;
    if (closing_paren_pos <= paren_pos)
      return false;

    line = line.substr(paren_pos + 1, closing_paren_pos - paren_pos - 1);
  }

  // Split the line into comma-separated pieces and extract
  // the last two.
  std::vector<base::StringPiece> pieces = base::SplitStringPiece(
      line, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  if (pieces.size() != 6)
    return false;

  // Ignore the IP address supplied in the response. We are always going
  // to connect back to the same server to prevent FTP PASV port scanning.
  uint32_t p0, p1;
  if (!ParseUint32(pieces[4], &p0))
    return false;
  if (!ParseUint32(pieces[5], &p1))
    return false;
  if (p0 > 0xFF || p1 > 0xFF)
    return false;

  *port = (p0 << 8) + p1;
  return true;
}

}  // namespace

FtpNetworkTransaction::FtpNetworkTransaction(
    HostResolver* resolver,
    ClientSocketFactory* socket_factory)
    : command_sent_(COMMAND_NONE),
      io_callback_(base::BindRepeating(&FtpNetworkTransaction::OnIOComplete,
                                       base::Unretained(this))),
      request_(nullptr),
      resolver_(resolver),
      read_ctrl_buf_(base::MakeRefCounted<IOBuffer>(kCtrlBufLen)),
      read_data_buf_len_(0),
      last_error_(OK),
      system_type_(SYSTEM_TYPE_UNKNOWN),
      // Use image (binary) transfer by default. It should always work,
      // whereas the ascii transfer may damage binary data.
      data_type_(DATA_TYPE_IMAGE),
      resource_type_(RESOURCE_TYPE_UNKNOWN),
      use_epsv_(true),
      data_connection_port_(0),
      socket_factory_(socket_factory),
      next_state_(STATE_NONE),
      state_after_data_connect_complete_(STATE_NONE) {}

FtpNetworkTransaction::~FtpNetworkTransaction() = default;

int FtpNetworkTransaction::Stop(int error) {
  if (command_sent_ == COMMAND_QUIT) {
    if (error != ERR_EMPTY_RESPONSE)
      return error;

    // For empty responses, if this is propagating an error, then it will return
    // the error. If the error occurred during a QUIT command, then this will
    // return OK since there was no previous error. Some FTP servers are lazy
    // and do not bother responding to QUIT commands.
    // See https://crbug.com/633841
    return last_error_;
  }

  next_state_ = STATE_CTRL_WRITE_QUIT;
  last_error_ = error;
  return OK;
}

int FtpNetworkTransaction::Start(
    const FtpRequestInfo* request_info,
    CompletionOnceCallback callback,
    const NetLogWithSource& net_log,
    const NetworkTrafficAnnotationTag& traffic_annotation) {
  net_log_ = net_log;
  request_ = request_info;
  traffic_annotation_ = MutableNetworkTrafficAnnotationTag(traffic_annotation);

  ctrl_response_buffer_ = std::make_unique<FtpCtrlResponseBuffer>(net_log_);

  if (request_->url.has_username()) {
    base::string16 username;
    base::string16 password;
    GetIdentityFromURL(request_->url, &username, &password);
    credentials_.Set(username, password);
  } else {
    credentials_.Set(base::ASCIIToUTF16("anonymous"),
                     base::ASCIIToUTF16("chrome@example.com"));
  }

  DetectTypecode();

  if (request_->url.has_path()) {
    std::string gurl_path(request_->url.path());

    // Get rid of the typecode, see RFC 1738 section 3.2.2. FTP url-path.
    std::string::size_type pos = gurl_path.rfind(';');
    if (pos != std::string::npos)
      gurl_path.resize(pos);

    // This may unescape to non-ASCII characters, but we allow that. See the
    // comment for IsValidFTPCommandSubstring.
    if (!UnescapeBinaryURLComponentSafe(
            gurl_path, true /* fail_on_path_separators*/, &unescaped_path_)) {
      return ERR_INVALID_URL;
    }
  }

  next_state_ = STATE_CTRL_RESOLVE_HOST;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = std::move(callback);
  return rv;
}

int FtpNetworkTransaction::RestartWithAuth(const AuthCredentials& credentials,
                                           CompletionOnceCallback callback) {
  ResetStateForRestart();

  credentials_ = credentials;

  next_state_ = STATE_CTRL_RESOLVE_HOST;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = std::move(callback);
  return rv;
}

int FtpNetworkTransaction::Read(IOBuffer* buf,
                                int buf_len,
                                CompletionOnceCallback callback) {
  DCHECK(buf);
  DCHECK_GT(buf_len, 0);

  read_data_buf_ = buf;
  read_data_buf_len_ = buf_len;

  next_state_ = STATE_DATA_READ;
  int rv = DoLoop(OK);
  if (rv == ERR_IO_PENDING)
    user_callback_ = std::move(callback);
  return rv;
}

const FtpResponseInfo* FtpNetworkTransaction::GetResponseInfo() const {
  return &response_;
}

LoadState FtpNetworkTransaction::GetLoadState() const {
  if (next_state_ == STATE_CTRL_RESOLVE_HOST_COMPLETE)
    return LOAD_STATE_RESOLVING_HOST;

  if (next_state_ == STATE_CTRL_CONNECT_COMPLETE ||
      next_state_ == STATE_DATA_CONNECT_COMPLETE)
    return LOAD_STATE_CONNECTING;

  if (next_state_ == STATE_DATA_READ_COMPLETE)
    return LOAD_STATE_READING_RESPONSE;

  if (command_sent_ == COMMAND_RETR && read_data_buf_.get())
    return LOAD_STATE_READING_RESPONSE;

  if (command_sent_ == COMMAND_QUIT)
    return LOAD_STATE_IDLE;

  if (command_sent_ != COMMAND_NONE)
    return LOAD_STATE_SENDING_REQUEST;

  return LOAD_STATE_IDLE;
}

uint64_t FtpNetworkTransaction::GetUploadProgress() const {
  return 0;
}

void FtpNetworkTransaction::ResetStateForRestart() {
  command_sent_ = COMMAND_NONE;
  user_callback_.Reset();
  response_ = FtpResponseInfo();
  read_ctrl_buf_ = base::MakeRefCounted<IOBuffer>(kCtrlBufLen);
  ctrl_response_buffer_ = std::make_unique<FtpCtrlResponseBuffer>(net_log_);
  read_data_buf_ = nullptr;
  read_data_buf_len_ = 0;
  if (write_buf_.get())
    write_buf_->SetOffset(0);
  last_error_ = OK;
  data_connection_port_ = 0;
  ctrl_socket_.reset();
  data_socket_.reset();
  next_state_ = STATE_NONE;
  state_after_data_connect_complete_ = STATE_NONE;
}

void FtpNetworkTransaction::EstablishDataConnection(State state_after_connect) {
  DCHECK(state_after_connect == STATE_CTRL_WRITE_RETR ||
         state_after_connect == STATE_CTRL_WRITE_LIST);
  state_after_data_connect_complete_ = state_after_connect;
  next_state_ = use_epsv_ ? STATE_CTRL_WRITE_EPSV : STATE_CTRL_WRITE_PASV;
}

void FtpNetworkTransaction::DoCallback(int rv) {
  DCHECK(rv != ERR_IO_PENDING);

  std::move(user_callback_).Run(rv);
}

void FtpNetworkTransaction::OnIOComplete(int result) {
  int rv = DoLoop(result);
  if (rv != ERR_IO_PENDING)
    DoCallback(rv);
}

int FtpNetworkTransaction::ProcessCtrlResponse() {
  FtpCtrlResponse response = ctrl_response_buffer_->PopResponse();

  int rv = OK;
  switch (command_sent_) {
    case COMMAND_NONE:
      // TODO(phajdan.jr): https://crbug.com/526721: Check for errors in the
      // welcome message.
      next_state_ = STATE_CTRL_WRITE_USER;
      break;
    case COMMAND_USER:
      rv = ProcessResponseUSER(response);
      break;
    case COMMAND_PASS:
      rv = ProcessResponsePASS(response);
      break;
    case COMMAND_SYST:
      rv = ProcessResponseSYST(response);
      break;
    case COMMAND_PWD:
      rv = ProcessResponsePWD(response);
      break;
    case COMMAND_TYPE:
      rv = ProcessResponseTYPE(response);
      break;
    case COMMAND_EPSV:
      rv = ProcessResponseEPSV(response);
      break;
    case COMMAND_PASV:
      rv = ProcessResponsePASV(response);
      break;
    case COMMAND_SIZE:
      rv = ProcessResponseSIZE(response);
      break;
    case COMMAND_RETR:
      rv = ProcessResponseRETR(response);
      break;
    case COMMAND_CWD:
      rv = ProcessResponseCWD(response);
      break;
    case COMMAND_LIST:
      rv = ProcessResponseLIST(response);
      break;
    case COMMAND_QUIT:
      rv = ProcessResponseQUIT(response);
      break;
    default:
      LOG(DFATAL) << "Unexpected value of command_sent_: " << command_sent_;
      return ERR_UNEXPECTED;
  }

  // We may get multiple responses for some commands,
  // see http://crbug.com/18036. Consume all responses, regardless of whether
  // they make sense. On unexpected responses, SendFtpCommand expects all data
  // to have already been consumed, even when sending the QUIT command.
  while (ctrl_response_buffer_->ResponseAvailable() && rv == OK) {
    response = ctrl_response_buffer_->PopResponse();

    switch (command_sent_) {
      case COMMAND_RETR:
        rv = ProcessResponseRETR(response);
        break;
      case COMMAND_LIST:
        rv = ProcessResponseLIST(response);
        break;
      default:
        // Multiple responses for other commands are invalid.
        rv = Stop(ERR_INVALID_RESPONSE);
        break;
    }
  }

  return rv;
}

// Used to prepare and send FTP command.
int FtpNetworkTransaction::SendFtpCommand(const std::string& command,
                                          const std::string& command_for_log,
                                          Command cmd) {
  // If we send a new command when we still have unprocessed responses
  // for previous commands, the response receiving code will have no way to know
  // which responses are for which command.
  DCHECK(!ctrl_response_buffer_->ResponseAvailable());

  DCHECK(!write_command_buf_.get());
  DCHECK(!write_buf_.get());

  if (!IsValidFTPCommandSubstring(command)) {
    // Callers should validate the command themselves and return a more specific
    // error code.
    NOTREACHED();
    return Stop(ERR_UNEXPECTED);
  }

  command_sent_ = cmd;

  write_command_buf_ =
      base::MakeRefCounted<IOBufferWithSize>(command.length() + 2);
  write_buf_ = base::MakeRefCounted<DrainableIOBuffer>(
      write_command_buf_, write_command_buf_->size());
  memcpy(write_command_buf_->data(), command.data(), command.length());
  memcpy(write_command_buf_->data() + command.length(), kCRLF, 2);

  net_log_.AddEventWithStringParams(NetLogEventType::FTP_COMMAND_SENT,
                                    "command", command_for_log);

  next_state_ = STATE_CTRL_WRITE;
  return OK;
}

std::string FtpNetworkTransaction::GetRequestPathForFtpCommand(
    bool is_directory) const {
  std::string path(current_remote_directory_ + unescaped_path_);

  // Make sure that if the path is expected to be a file, it won't end
  // with a trailing slash.
  if (!is_directory && path.length() > 1 && path.back() == '/')
    path.erase(path.length() - 1);

  if (system_type_ == SYSTEM_TYPE_VMS) {
    if (is_directory)
      path = FtpUtil::UnixDirectoryPathToVMS(path);
    else
      path = FtpUtil::UnixFilePathToVMS(path);
  }

  DCHECK(IsValidFTPCommandSubstring(path));
  return path;
}

void FtpNetworkTransaction::DetectTypecode() {
  if (!request_->url.has_path())
    return;
  std::string gurl_path(request_->url.path());

  // Extract the typecode, see RFC 1738 section 3.2.2. FTP url-path.
  std::string::size_type pos = gurl_path.rfind(';');
  if (pos == std::string::npos)
    return;
  std::string typecode_string(gurl_path.substr(pos));
  if (typecode_string == ";type=a") {
    data_type_ = DATA_TYPE_ASCII;
    resource_type_ = RESOURCE_TYPE_FILE;
  } else if (typecode_string == ";type=i") {
    data_type_ = DATA_TYPE_IMAGE;
    resource_type_ = RESOURCE_TYPE_FILE;
  } else if (typecode_string == ";type=d") {
    resource_type_ = RESOURCE_TYPE_DIRECTORY;
  }
}

int FtpNetworkTransaction::DoLoop(int result) {
  DCHECK(next_state_ != STATE_NONE);

  int rv = result;
  do {
    State state = next_state_;
    next_state_ = STATE_NONE;
    switch (state) {
      case STATE_CTRL_RESOLVE_HOST:
        DCHECK(rv == OK);
        rv = DoCtrlResolveHost();
        break;
      case STATE_CTRL_RESOLVE_HOST_COMPLETE:
        rv = DoCtrlResolveHostComplete(rv);
        break;
      case STATE_CTRL_CONNECT:
        DCHECK(rv == OK);
        rv = DoCtrlConnect();
        break;
      case STATE_CTRL_CONNECT_COMPLETE:
        rv = DoCtrlConnectComplete(rv);
        break;
      case STATE_CTRL_READ:
        DCHECK(rv == OK);
        rv = DoCtrlRead();
        break;
      case STATE_CTRL_READ_COMPLETE:
        rv = DoCtrlReadComplete(rv);
        break;
      case STATE_CTRL_WRITE:
        DCHECK(rv == OK);
        rv = DoCtrlWrite();
        break;
      case STATE_CTRL_WRITE_COMPLETE:
        rv = DoCtrlWriteComplete(rv);
        break;
      case STATE_CTRL_WRITE_USER:
        DCHECK(rv == OK);
        rv = DoCtrlWriteUSER();
        break;
      case STATE_CTRL_WRITE_PASS:
        DCHECK(rv == OK);
        rv = DoCtrlWritePASS();
        break;
      case STATE_CTRL_WRITE_SYST:
        DCHECK(rv == OK);
        rv = DoCtrlWriteSYST();
        break;
      case STATE_CTRL_WRITE_PWD:
        DCHECK(rv == OK);
        rv = DoCtrlWritePWD();
        break;
      case STATE_CTRL_WRITE_TYPE:
        DCHECK(rv == OK);
        rv = DoCtrlWriteTYPE();
        break;
      case STATE_CTRL_WRITE_EPSV:
        DCHECK(rv == OK);
        rv = DoCtrlWriteEPSV();
        break;
      case STATE_CTRL_WRITE_PASV:
        DCHECK(rv == OK);
        rv = DoCtrlWritePASV();
        break;
      case STATE_CTRL_WRITE_RETR:
        DCHECK(rv == OK);
        rv = DoCtrlWriteRETR();
        break;
      case STATE_CTRL_WRITE_SIZE:
        DCHECK(rv == OK);
        rv = DoCtrlWriteSIZE();
        break;
      case STATE_CTRL_WRITE_CWD:
        DCHECK(rv == OK);
        rv = DoCtrlWriteCWD();
        break;
      case STATE_CTRL_WRITE_LIST:
        DCHECK(rv == OK);
        rv = DoCtrlWriteLIST();
        break;
      case STATE_CTRL_WRITE_QUIT:
        DCHECK(rv == OK);
        rv = DoCtrlWriteQUIT();
        break;
      case STATE_DATA_CONNECT:
        DCHECK(rv == OK);
        rv = DoDataConnect();
        break;
      case STATE_DATA_CONNECT_COMPLETE:
        rv = DoDataConnectComplete(rv);
        break;
      case STATE_DATA_READ:
        DCHECK(rv == OK);
        rv = DoDataRead();
        break;
      case STATE_DATA_READ_COMPLETE:
        rv = DoDataReadComplete(rv);
        break;
      default:
        NOTREACHED() << "bad state";
        rv = ERR_UNEXPECTED;
        break;
    }
  } while (rv != ERR_IO_PENDING && next_state_ != STATE_NONE);
  return rv;
}

int FtpNetworkTransaction::DoCtrlResolveHost() {
  next_state_ = STATE_CTRL_RESOLVE_HOST_COMPLETE;

  // Using an empty NetworkIsolationKey here, since FTP support is deprecated,
  // and should go away soon.
  resolve_request_ =
      resolver_->CreateRequest(HostPortPair::FromURL(request_->url),
                               NetworkIsolationKey(), net_log_, base::nullopt);
  return resolve_request_->Start(base::BindOnce(
      &FtpNetworkTransaction::OnIOComplete, base::Unretained(this)));
}

int FtpNetworkTransaction::DoCtrlResolveHostComplete(int result) {
  if (result == OK)
    next_state_ = STATE_CTRL_CONNECT;
  return result;
}

int FtpNetworkTransaction::DoCtrlConnect() {
  next_state_ = STATE_CTRL_CONNECT_COMPLETE;
  DCHECK(resolve_request_ && resolve_request_->GetAddressResults());
  ctrl_socket_ = socket_factory_->CreateTransportClientSocket(
      resolve_request_->GetAddressResults().value(), nullptr,
      net_log_.net_log(), net_log_.source());
  net_log_.AddEventReferencingSource(NetLogEventType::FTP_CONTROL_CONNECTION,
                                     ctrl_socket_->NetLog().source());
  return ctrl_socket_->Connect(io_callback_);
}

int FtpNetworkTransaction::DoCtrlConnectComplete(int result) {
  if (result == OK) {
    // Put the peer's IP address and port into the response.
    IPEndPoint ip_endpoint;
    result = ctrl_socket_->GetPeerAddress(&ip_endpoint);
    if (result == OK) {
      response_.remote_endpoint = ip_endpoint;
      next_state_ = STATE_CTRL_READ;

      if (ip_endpoint.GetFamily() == ADDRESS_FAMILY_IPV4) {
        // Do not use EPSV for IPv4 connections. Some servers become confused
        // and we time out while waiting to connect. PASV is perfectly fine for
        // IPv4. Note that this blocks IPv4 not to use EPSV instead of allowing
        // IPv6 to use it, to make the code more future-proof:
        // all future protocols should just use EPSV.
        use_epsv_ = false;
      }
    }
  }
  return result;
}

int FtpNetworkTransaction::DoCtrlRead() {
  next_state_ = STATE_CTRL_READ_COMPLETE;
  return ctrl_socket_->Read(read_ctrl_buf_.get(), kCtrlBufLen, io_callback_);
}

int FtpNetworkTransaction::DoCtrlReadComplete(int result) {
  if (result == 0) {
    // Some servers (for example Pure-FTPd) apparently close the control
    // connection when anonymous login is not permitted. For more details
    // see http://crbug.com/25023.
    if (command_sent_ == COMMAND_USER &&
        credentials_.username() == base::ASCIIToUTF16("anonymous")) {
      response_.needs_auth = true;
    }
    return Stop(ERR_EMPTY_RESPONSE);
  }
  if (result < 0)
    return Stop(result);

  ctrl_response_buffer_->ConsumeData(read_ctrl_buf_->data(), result);

  if (!ctrl_response_buffer_->ResponseAvailable()) {
    // Read more data from the control socket.
    next_state_ = STATE_CTRL_READ;
    return OK;
  }

  return ProcessCtrlResponse();
}

int FtpNetworkTransaction::DoCtrlWrite() {
  next_state_ = STATE_CTRL_WRITE_COMPLETE;

  return ctrl_socket_->Write(write_buf_.get(), write_buf_->BytesRemaining(),
                             io_callback_,
                             NetworkTrafficAnnotationTag(traffic_annotation_));
}

int FtpNetworkTransaction::DoCtrlWriteComplete(int result) {
  if (result < 0)
    return result;

  write_buf_->DidConsume(result);
  if (write_buf_->BytesRemaining() == 0) {
    // Clear the write buffer.
    write_buf_ = nullptr;
    write_command_buf_ = nullptr;

    next_state_ = STATE_CTRL_READ;
  } else {
    next_state_ = STATE_CTRL_WRITE;
  }
  return OK;
}

// FTP Commands and responses

// USER Command.
int FtpNetworkTransaction::DoCtrlWriteUSER() {
  std::string command = "USER " + base::UTF16ToUTF8(credentials_.username());

  if (!IsValidFTPCommandSubstring(command))
    return Stop(ERR_MALFORMED_IDENTITY);

  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, "USER ***", COMMAND_USER);
}

int FtpNetworkTransaction::ProcessResponseUSER(
    const FtpCtrlResponse& response) {
  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_SYST;
      break;
    case ERROR_CLASS_INFO_NEEDED:
      next_state_ = STATE_CTRL_WRITE_PASS;
      break;
    case ERROR_CLASS_TRANSIENT_ERROR:
    case ERROR_CLASS_PERMANENT_ERROR:
      response_.needs_auth = true;
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
  }
  return OK;
}

// PASS command.
int FtpNetworkTransaction::DoCtrlWritePASS() {
  std::string command = "PASS " + base::UTF16ToUTF8(credentials_.password());

  if (!IsValidFTPCommandSubstring(command))
    return Stop(ERR_MALFORMED_IDENTITY);

  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, "PASS ***", COMMAND_PASS);
}

int FtpNetworkTransaction::ProcessResponsePASS(
    const FtpCtrlResponse& response) {
  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_SYST;
      break;
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_TRANSIENT_ERROR:
    case ERROR_CLASS_PERMANENT_ERROR:
      response_.needs_auth = true;
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
  }
  return OK;
}

// SYST command.
int FtpNetworkTransaction::DoCtrlWriteSYST() {
  std::string command = "SYST";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_SYST);
}

int FtpNetworkTransaction::ProcessResponseSYST(
    const FtpCtrlResponse& response) {
  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_OK: {
      // All important info should be on the first line.
      std::string line = response.lines[0];
      // The response should be ASCII, which allows us to do case-insensitive
      // comparisons easily. If it is not ASCII, we leave the system type
      // as unknown.
      if (base::IsStringASCII(line)) {
        line = base::ToLowerASCII(line);

        // Remove all whitespace, to correctly handle cases like fancy "V M S"
        // response instead of "VMS".
        base::RemoveChars(line, base::kWhitespaceASCII, &line);

        // The "magic" strings we test for below have been gathered by an
        // empirical study. VMS needs to come first because some VMS systems
        // also respond with "UNIX emulation", which is not perfect. It is much
        // more reliable to talk to these servers in their native language.
        if (line.find("vms") != std::string::npos) {
          system_type_ = SYSTEM_TYPE_VMS;
        } else if (line.find("l8") != std::string::npos ||
                   line.find("unix") != std::string::npos ||
                   line.find("bsd") != std::string::npos) {
          system_type_ = SYSTEM_TYPE_UNIX;
        } else if (line.find("win32") != std::string::npos ||
                   line.find("windows") != std::string::npos) {
          system_type_ = SYSTEM_TYPE_WINDOWS;
        } else if (line.find("os/2") != std::string::npos) {
          system_type_ = SYSTEM_TYPE_OS2;
        }
      }
      next_state_ = STATE_CTRL_WRITE_PWD;
      break;
    }
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_TRANSIENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_PERMANENT_ERROR:
      // Server does not recognize the SYST command so proceed.
      next_state_ = STATE_CTRL_WRITE_PWD;
      break;
  }
  return OK;
}

// PWD command.
int FtpNetworkTransaction::DoCtrlWritePWD() {
  std::string command = "PWD";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_PWD);
}

int FtpNetworkTransaction::ProcessResponsePWD(const FtpCtrlResponse& response) {
  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_OK: {
      // The info we look for should be on the first line.
      std::string line = response.lines[0];
      if (line.empty())
        return Stop(ERR_INVALID_RESPONSE);
      std::string::size_type quote_pos = line.find('"');
      if (quote_pos != std::string::npos) {
        line = line.substr(quote_pos + 1);
        quote_pos = line.find('"');
        if (quote_pos == std::string::npos)
          return Stop(ERR_INVALID_RESPONSE);
        line = line.substr(0, quote_pos);
      }
      if (system_type_ == SYSTEM_TYPE_VMS)
        line = FtpUtil::VMSPathToUnix(line);
      if (!line.empty() && line.back() == '/')
        line.erase(line.length() - 1);
      // Fail if the "path" contains characters not allowed in commands.
      // This does mean that files with CRs or LFs in their names aren't
      // handled, but that's probably for the best.
      if (!IsValidFTPCommandSubstring(line))
        return Stop(ERR_INVALID_RESPONSE);
      current_remote_directory_ = line;
      next_state_ = STATE_CTRL_WRITE_TYPE;
      break;
    }
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_TRANSIENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_PERMANENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
  }
  return OK;
}

// TYPE command.
int FtpNetworkTransaction::DoCtrlWriteTYPE() {
  std::string command = "TYPE ";
  if (data_type_ == DATA_TYPE_ASCII) {
    command += "A";
  } else if (data_type_ == DATA_TYPE_IMAGE) {
    command += "I";
  } else {
    NOTREACHED();
    return Stop(ERR_UNEXPECTED);
  }
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_TYPE);
}

int FtpNetworkTransaction::ProcessResponseTYPE(
    const FtpCtrlResponse& response) {
  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_SIZE;
      break;
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_TRANSIENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_PERMANENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
  }
  return OK;
}

// EPSV command
int FtpNetworkTransaction::DoCtrlWriteEPSV() {
  const std::string command = "EPSV";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_EPSV);
}

int FtpNetworkTransaction::ProcessResponseEPSV(
    const FtpCtrlResponse& response) {
  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_OK: {
      int port;
      if (!ExtractPortFromEPSVResponse(response, &port))
        return Stop(ERR_INVALID_RESPONSE);
      if (IsWellKnownPort(port) ||
          !IsPortAllowedForScheme(port, url::kFtpScheme)) {
        return Stop(ERR_UNSAFE_PORT);
      }
      data_connection_port_ = static_cast<uint16_t>(port);
      next_state_ = STATE_DATA_CONNECT;
      break;
    }
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_TRANSIENT_ERROR:
    case ERROR_CLASS_PERMANENT_ERROR:
      use_epsv_ = false;
      next_state_ = STATE_CTRL_WRITE_PASV;
      return OK;
  }
  return OK;
}

// PASV command
int FtpNetworkTransaction::DoCtrlWritePASV() {
  std::string command = "PASV";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_PASV);
}

int FtpNetworkTransaction::ProcessResponsePASV(
    const FtpCtrlResponse& response) {
  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_OK: {
      int port;
      if (!ExtractPortFromPASVResponse(response, &port))
        return Stop(ERR_INVALID_RESPONSE);
      if (IsWellKnownPort(port) ||
          !IsPortAllowedForScheme(port, url::kFtpScheme)) {
        return Stop(ERR_UNSAFE_PORT);
      }
      data_connection_port_ = static_cast<uint16_t>(port);
      next_state_ = STATE_DATA_CONNECT;
      break;
    }
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_TRANSIENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_PERMANENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
  }
  return OK;
}

// RETR command
int FtpNetworkTransaction::DoCtrlWriteRETR() {
  std::string command = "RETR " + GetRequestPathForFtpCommand(false);
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_RETR);
}

int FtpNetworkTransaction::ProcessResponseRETR(
    const FtpCtrlResponse& response) {
  // Resource type should be either filled in by DetectTypecode() or
  // detected with CWD. RETR is sent only when the resource is a file.
  DCHECK_EQ(RESOURCE_TYPE_FILE, resource_type_);

  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      // We want the client to start reading the response at this point.
      // It got here either through Start or RestartWithAuth. We want that
      // method to complete. Not setting next state here will make DoLoop exit
      // and in turn make Start/RestartWithAuth complete.
      break;
    case ERROR_CLASS_OK:
      next_state_ = STATE_CTRL_WRITE_QUIT;
      break;
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_TRANSIENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_PERMANENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
  }

  return OK;
}

// SIZE command
int FtpNetworkTransaction::DoCtrlWriteSIZE() {
  std::string command = "SIZE " + GetRequestPathForFtpCommand(false);
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_SIZE);
}

int FtpNetworkTransaction::ProcessResponseSIZE(
    const FtpCtrlResponse& response) {
  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      break;
    case ERROR_CLASS_OK:
      if (response.lines.size() != 1)
        return Stop(ERR_INVALID_RESPONSE);
      int64_t size;
      if (!base::StringToInt64(response.lines[0], &size))
        return Stop(ERR_INVALID_RESPONSE);
      if (size < 0)
        return Stop(ERR_INVALID_RESPONSE);

      // A successful response to SIZE does not mean the resource is a file.
      // Some FTP servers (for example, the qnx one) send a SIZE even for
      // directories.
      response_.expected_content_size = size;
      break;
    case ERROR_CLASS_INFO_NEEDED:
      break;
    case ERROR_CLASS_TRANSIENT_ERROR:
      break;
    case ERROR_CLASS_PERMANENT_ERROR:
      // It's possible that SIZE failed because the path is a directory.
      // TODO(xunjieli): https://crbug.com/526724: Add a test for this case.
      if (resource_type_ == RESOURCE_TYPE_UNKNOWN &&
          response.status_code != 550) {
        return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
      }
      break;
  }

  // If the resource is known beforehand to be a file, RETR should be issued,
  // otherwise do CWD which will detect the resource type.
  if (resource_type_ == RESOURCE_TYPE_FILE)
    EstablishDataConnection(STATE_CTRL_WRITE_RETR);
  else
    next_state_ = STATE_CTRL_WRITE_CWD;
  return OK;
}

// CWD command
int FtpNetworkTransaction::DoCtrlWriteCWD() {
  std::string command = "CWD " + GetRequestPathForFtpCommand(true);
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_CWD);
}

int FtpNetworkTransaction::ProcessResponseCWD(const FtpCtrlResponse& response) {
  // CWD should be invoked only when the resource is not a file.
  DCHECK_NE(RESOURCE_TYPE_FILE, resource_type_);

  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_OK:
      resource_type_ = RESOURCE_TYPE_DIRECTORY;
      EstablishDataConnection(STATE_CTRL_WRITE_LIST);
      break;
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_TRANSIENT_ERROR:
      // Some FTP servers send response 451 (not a valid CWD response according
      // to RFC 959) instead of 550.
      if (response.status_code == 451)
        return ProcessResponseCWDNotADirectory();

      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_PERMANENT_ERROR:
      if (response.status_code == 550)
        return ProcessResponseCWDNotADirectory();

      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
  }

  return OK;
}

int FtpNetworkTransaction::ProcessResponseCWDNotADirectory() {
  if (resource_type_ == RESOURCE_TYPE_DIRECTORY) {
    // We're assuming that the resource is a directory, but the server
    // says it's not true. The most probable interpretation is that it
    // doesn't exist (with FTP we can't be sure).
    return Stop(ERR_FILE_NOT_FOUND);
  }

  // If it is not a directory, it is probably a file.
  resource_type_ = RESOURCE_TYPE_FILE;

  EstablishDataConnection(STATE_CTRL_WRITE_RETR);
  return OK;
}

// LIST command
int FtpNetworkTransaction::DoCtrlWriteLIST() {
  // Use the -l option for mod_ftp configured in LISTIsNLST mode: the option
  // forces LIST output instead of NLST (which would be ambiguous for us
  // to parse).
  std::string command("LIST -l");
  if (system_type_ == SYSTEM_TYPE_VMS)
    command = "LIST *.*;0";

  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_LIST);
}

int FtpNetworkTransaction::ProcessResponseLIST(
    const FtpCtrlResponse& response) {
  // Resource type should be either filled in by DetectTypecode() or
  // detected with CWD. LIST is sent only when the resource is a directory.
  DCHECK_EQ(RESOURCE_TYPE_DIRECTORY, resource_type_);

  switch (GetErrorClass(response.status_code)) {
    case ERROR_CLASS_INITIATED:
      // We want the client to start reading the response at this point.
      // It got here either through Start or RestartWithAuth. We want that
      // method to complete. Not setting next state here will make DoLoop exit
      // and in turn make Start/RestartWithAuth complete.
      response_.is_directory_listing = true;
      break;
    case ERROR_CLASS_OK:
      response_.is_directory_listing = true;
      next_state_ = STATE_CTRL_WRITE_QUIT;
      break;
    case ERROR_CLASS_INFO_NEEDED:
      return Stop(ERR_INVALID_RESPONSE);
    case ERROR_CLASS_TRANSIENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
    case ERROR_CLASS_PERMANENT_ERROR:
      return Stop(GetNetErrorCodeForFtpResponseCode(response.status_code));
  }
  return OK;
}

// QUIT command
int FtpNetworkTransaction::DoCtrlWriteQUIT() {
  std::string command = "QUIT";
  next_state_ = STATE_CTRL_READ;
  return SendFtpCommand(command, command, COMMAND_QUIT);
}

int FtpNetworkTransaction::ProcessResponseQUIT(
    const FtpCtrlResponse& response) {
  ctrl_socket_->Disconnect();
  return last_error_;
}

// Data Connection

int FtpNetworkTransaction::DoDataConnect() {
  next_state_ = STATE_DATA_CONNECT_COMPLETE;
  IPEndPoint ip_endpoint;
  AddressList data_address;
  // Connect to the same host as the control socket to prevent PASV port
  // scanning attacks.
  int rv = ctrl_socket_->GetPeerAddress(&ip_endpoint);
  if (rv != OK)
    return Stop(rv);
  data_address = AddressList::CreateFromIPAddress(
      ip_endpoint.address(), data_connection_port_);
  data_socket_ = socket_factory_->CreateTransportClientSocket(
      data_address, nullptr, net_log_.net_log(), net_log_.source());
  net_log_.AddEventReferencingSource(NetLogEventType::FTP_DATA_CONNECTION,
                                     data_socket_->NetLog().source());
  return data_socket_->Connect(io_callback_);
}

int FtpNetworkTransaction::DoDataConnectComplete(int result) {
  if (result != OK && use_epsv_) {
    // It's possible we hit a broken server, sadly. They can break in different
    // ways. Some time out, some reset a connection. Fall back to PASV.
    // TODO(phajdan.jr): https://crbug.com/526723: remember it for future
    // transactions with this server.
    use_epsv_ = false;
    next_state_ = STATE_CTRL_WRITE_PASV;
    return OK;
  }

  if (result != OK)
    return Stop(result);

  next_state_ = state_after_data_connect_complete_;
  return OK;
}

int FtpNetworkTransaction::DoDataRead() {
  DCHECK(read_data_buf_.get());
  DCHECK_GT(read_data_buf_len_, 0);

  if (!data_socket_ || !data_socket_->IsConnected()) {
    // If we don't destroy the data socket completely, some servers will wait
    // for us (http://crbug.com/21127). The half-closed TCP connection needs
    // to be closed on our side too.
    data_socket_.reset();

    if (ctrl_socket_->IsConnected()) {
      // Wait for the server's response, we should get it before sending QUIT.
      next_state_ = STATE_CTRL_READ;
      return OK;
    }

    // We are no longer connected to the server, so just finish the transaction.
    return Stop(OK);
  }

  next_state_ = STATE_DATA_READ_COMPLETE;
  read_data_buf_->data()[0] = 0;
  return data_socket_->Read(
      read_data_buf_.get(), read_data_buf_len_, io_callback_);
}

int FtpNetworkTransaction::DoDataReadComplete(int result) {
  return result;
}

}  // namespace net
